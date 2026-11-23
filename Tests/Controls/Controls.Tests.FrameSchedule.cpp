#include "Controls.Tests.FrameSchedule.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // scenarios and ports are never copied
#include "Embedded.Tests.PerformanceFixtureFx1.h"
#pragma warning(pop)
#include "GraphicsFixture.h"
#include "Support.Tests.FailureHost.h"
#include "Support/AnimationDispatcher.h"
#include "Support/WorkCounters.h"

#include <array>
#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <thread>
#include <vector>

// Every clause of W3.1's gate that does not need the fixture runner. The fixture clauses (FX1 through the fixture clock,
// FX5's ticks per wake) are in Embedded.Tests.PerformanceFixtures.h and Controls.Tests.PerformanceFixtures.cpp.
namespace
{
using DxUi::Detail::WorkCounters;
using DxUi::Detail::WorkCounterScope;
using DxUi::Ui::AnimationDispatcher;

// A control whose Tick runs a test's function: it counts its ticks and answers what the function answers.
class ScheduleProbe final : public DxUi::Control
{
public:
    ScheduleProbe()                                = default;
    ScheduleProbe(const ScheduleProbe&)            = delete;
    ScheduleProbe& operator=(const ScheduleProbe&) = delete;
    ScheduleProbe(ScheduleProbe&&)                 = delete;
    ScheduleProbe& operator=(ScheduleProbe&&)      = delete;
    ~ScheduleProbe() override
    {
        if (destroyed)
            *destroyed = true;
    }
    void Paint(DxUi::ControlHost&) const override
    {
    }
    bool Tick(DxUi::ControlHost& host, uint64_t nowTickMs) override
    {
        ++ticks;
        lastTickMs = nowTickMs;
        // The function may destroy this probe: nothing below reads a member after it.
        return onTick ? onTick(host, nowTickMs) : false;
    }

    size_t ticks        = 0u;
    uint64_t lastTickMs = 0u;
    bool* destroyed     = nullptr;
    std::function<bool(DxUi::ControlHost&, uint64_t)> onTick;
};

[[nodiscard]] std::shared_ptr<DxUi::GraphicsDevice> MakeSchedulePool(GraphicsFixture& gpu)
{
    RequireSucceeded(gpu.Create(), "create the frame-schedule WARP device");
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    RequireSucceeded(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "share the frame-schedule graphics");
    return graphics;
}

[[nodiscard]] uint64_t TimerWakes(const WorkCounters& delta) noexcept
{
    return delta.wakes.animationTimer + delta.wakes.deadlineTimer;
}

// Pumps this thread's messages for `durationMs` of real time, blocking between them (no busy polling).
void PumpFor(uint64_t durationMs)
{
    const uint64_t end = GetTickCount64() + durationMs;
    for (uint64_t now = GetTickCount64(); now < end; now = GetTickCount64())
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
            static_cast<void>(DispatchQueuedMessageForTest(message));
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, static_cast<DWORD>(end - now), QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
}

template <class Condition> [[nodiscard]] bool PumpUntil(uint64_t limitMs, Condition&& reached)
{
    const uint64_t end = GetTickCount64() + limitMs;
    for (uint64_t now = GetTickCount64(); now < end; now = GetTickCount64())
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
        {
            static_cast<void>(DispatchQueuedMessageForTest(message));
            if (reached())
                return true;
        }
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, static_cast<DWORD>(end - now), QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
    return reached();
}

// A port for building a fixture scene outside its runner: the scene's Build and its focus step never use it.
class IdlePort final : public PerformanceFixtures::HostPort
{
public:
    IdlePort()                           = default;
    IdlePort(const IdlePort&)            = delete;
    IdlePort& operator=(const IdlePort&) = delete;
    IdlePort(IdlePort&&)                 = delete;
    IdlePort& operator=(IdlePort&&)      = delete;
    ~IdlePort() override                 = default;
    bool Pointer(const DxUi::PointerEvent&) override
    {
        return false;
    }
    bool Key(UINT, bool, UINT) override
    {
        return false;
    }
    bool Character(wchar_t, UINT) override
    {
        return false;
    }
    void AdvanceTime(uint64_t) override
    {
    }
    void Resize(UINT, UINT, float) override
    {
    }
    [[nodiscard]] uint64_t NowMs() const noexcept override
    {
        return 0u;
    }
};
} // namespace

// FX5's clause on a scene that moves: a host with many controls ticks only the controls holding a due request, a
// deadline ticks nothing before it is due while the request stays that deadline, and no wake walks the tree.
void TestFrameScheduleTicksOnlyDueRequesters()
{
    GraphicsFixture gpu;
    const auto pool = MakeSchedulePool(gpu);
    DxUi::EmbeddedHost view;
    RequireSucceeded(view.Attach(pool, StoppedTestClock()), "attach the frame-schedule view");
    view.Controls().SetTheme(MakeAnimatedTestThemePalette(false));
    auto root = std::make_unique<DxUi::Panel>();
    for (int index = 0; index < 40; ++index)
        root->AddChild<DxUi::Label>(L"Static")->SetBounds(D2D1::RectF(0.0f, 4.0f * static_cast<float>(index), 120.0f, 4.0f * static_cast<float>(index) + 4.0f));
    auto* continuous = root->AddChild<ScheduleProbe>();
    auto* deadline   = root->AddChild<ScheduleProbe>();
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(view.Prepare(320u, 200u), "prepare the frame-schedule view");
    uint64_t tick = 5'000u;
    {
        const WorkCounterScope discovery;
        static_cast<void>(view.AdvanceAnimation(tick));
        Require(discovery.Delta().frame.tickDiscoveryWalks == 1u && discovery.Delta().frame.tickLegacyTreeWalks == 0u,
                "a new embedded root gets one discovery walk, not a legacy one");
    }
    Require(! AnimationRequested(view.GetAnimationRequest()), "nothing requests after the discovery walk");
    continuous->ticks  = 0u;
    deadline->ticks    = 0u;
    continuous->onTick = [continuous](DxUi::ControlHost&, uint64_t) { return continuous->ticks < 3u; };

    const uint64_t start = view.Controls().GetAnimationTickMs();
    view.Controls().RequestAnimation(*continuous);
    view.Controls().RequestAnimationAt(*deadline, start + 500u);
    Require(AnimationRequested(view.GetAnimationRequest()), "the requests ask for ticks");
    for (int frame = 0; frame < 10; ++frame)
    {
        const WorkCounterScope wake;
        tick += 16u;
        static_cast<void>(view.AdvanceAnimation(tick));
        const WorkCounters delta = wake.Delta();
        Require(delta.frame.tickCalls == delta.frame.tickRequesters, "every wake ticks exactly the due requesters");
        Require(delta.frame.treeVisitsTick == 0u && delta.frame.tickLegacyTreeWalks == 0u && delta.frame.tickDiscoveryWalks == 0u, "no wake walks the tree");
        Require(delta.frame.tickCalls == (frame < 3 ? 1u : 0u), "only the continuous requester ticks, until it returns false");
    }
    Require(continuous->ticks == 3u && deadline->ticks == 0u, "the deadline does not tick before it is due");
    Require(AnimationRequested(view.GetAnimationRequest()), "the request stays a deadline while it is pending");
    const uint64_t due = start + 500u - view.Controls().GetAnimationTickMs();
    tick += due - 1u;
    static_cast<void>(view.AdvanceAnimation(tick));
    Require(deadline->ticks == 0u && AnimationRequested(view.GetAnimationRequest()), "one millisecond early ticks nothing");
    tick += 1u;
    {
        const WorkCounterScope wake;
        Require(! AnimationRequested(view.AdvanceAnimation(tick)), "the last request is consumed");
        Require(wake.Delta().frame.tickCalls == 1u && deadline->ticks == 1u, "the due deadline ticks its control once");
    }
    Require(! AnimationRequested(view.GetAnimationRequest()), "nothing is scheduled after the deadline");
}

// Compatibility: a derived control that only returns true from Tick, woken by RequestAnimation() without naming itself,
// still animates on both hosts; the walk happens once and the control then holds a request of its own.
void TestFrameScheduleLegacyRequestStillAnimatesDerivedControl()
{
    {
        AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
        auto root     = std::make_unique<DxUi::Panel>();
        auto* probe   = root->AddChild<ScheduleProbe>();
        probe->onTick = [probe](DxUi::ControlHost&, uint64_t) { return probe->ticks < 5u; };
        window.Host().SetRoot(std::move(root));
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        const WorkCounterScope legacy;
        window.Host().RequestAnimation();
        Require(window.Host().DebugHasActiveAnimationSubscription(), "the anonymous request arms the dispatcher");
        uint64_t tick = 1'000u;
        while (window.Host().DebugAnimationTickForTest(tick))
            tick += 16u;
        const WorkCounters delta = legacy.Delta();
        Require(probe->ticks == 5u, "the derived control animates until its Tick returns false");
        Require(delta.frame.tickLegacyTreeWalks == 1u && delta.frame.tickCalls == 4u, "one legacy walk, then per-control ticks");
        Require(! window.Host().DebugHasActiveAnimationSubscription(), "the host stops waking when the control stops");
    }
    {
        GraphicsFixture gpu;
        const auto pool = MakeSchedulePool(gpu);
        DxUi::EmbeddedHost view;
        RequireSucceeded(view.Attach(pool, StoppedTestClock()), "attach the legacy view");
        auto root   = std::make_unique<DxUi::Panel>();
        auto* probe = root->AddChild<ScheduleProbe>();
        view.Controls().SetRoot(std::move(root));
        RequireSucceeded(view.Prepare(200u, 100u), "prepare the legacy view");
        static_cast<void>(view.AdvanceAnimation(10u)); // the discovery walk
        probe->ticks  = 0u;
        probe->onTick = [probe](DxUi::ControlHost&, uint64_t) { return probe->ticks < 4u; };
        const WorkCounterScope legacy;
        view.Controls().RequestAnimation();
        uint64_t tick = 100u;
        while (AnimationRequested(view.GetAnimationRequest()))
            static_cast<void>(view.AdvanceAnimation(tick += 16u));
        Require(probe->ticks == 4u && legacy.Delta().frame.tickLegacyTreeWalks == 1u, "embedded: the derived control animates through one legacy walk");
    }
}

// Hidden, minimized and presentation-hidden hosts hold their schedule and are not woken; a deadline that came due
// meanwhile fires once on show.
void TestFrameScheduleHiddenHostHoldsItsSchedule()
{
    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host  = window.Host();
    auto root   = std::make_unique<DxUi::Panel>();
    auto* probe = root->AddChild<ScheduleProbe>();
    host.SetRoot(std::move(root));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    host.DebugSetAnimationClockForTest(10'000u);

    host.RequestAnimationAt(*probe, 10'500u);
    Require(host.DebugHasActiveAnimationSubscription() && host.DebugGetFrameScheduleWakeForTest().kind == 2u &&
                host.DebugGetFrameScheduleWakeForTest().dueTickMs == 10'500u,
            "a deadline request arms the dispatcher for its deadline");
    Require(host.SetNativePresentationVisible(false) == S_OK && ! host.DebugHasActiveAnimationSubscription() &&
                host.DebugGetFrameScheduleWakeForTest().kind == 0u,
            "hiding the presentation stops the host's wakes");
    Require(! host.DebugAnimationTickForTest(11'000u) && probe->ticks == 0u, "a hidden host ticks nothing, even past the deadline");
    host.DebugSetAnimationClockForTest(11'000u);
    {
        const WorkCounterScope show;
        Require(host.SetNativePresentationVisible(true) == S_OK && host.DebugHasActiveAnimationSubscription(), "showing resumes the held schedule");
        Require(show.Delta().frame.tickHeldWhileHidden == 1u, "the expired deadline was held");
        Require(! host.DebugAnimationTickForTest(11'000u) && probe->ticks == 1u, "and fires once on show");
    }

    host.RequestAnimationAt(*probe, 12'000u);
    ShowWindow(window.Hwnd(), SW_SHOWMINNOACTIVE);
    Require(! host.DebugHasActiveAnimationSubscription(), "minimizing stops the host's wakes at once");
    Require(! host.DebugAnimationTickForTest(13'000u) && probe->ticks == 1u, "a minimized host ticks nothing");
    host.DebugSetAnimationClockForTest(13'000u);
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    Require(host.DebugHasActiveAnimationSubscription(), "restoring resumes the held schedule");
    Require(! host.DebugAnimationTickForTest(13'000u) && probe->ticks == 2u, "the held deadline fires once after the restore");

    host.RequestAnimation(*probe);
    probe->onTick = [](DxUi::ControlHost&, uint64_t) { return true; };
    ShowWindow(window.Hwnd(), SW_HIDE);
    Require(! host.DebugAnimationTickForTest(14'000u) && probe->ticks == 2u && ! host.DebugHasActiveAnimationSubscription(),
            "a hidden window is observed at its first wake and holds even a continuous request");
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    Require(host.DebugAnimationTickForTest(14'016u) && probe->ticks == 3u, "the continuous request resumes on show");
    host.CancelAnimation(*probe);
    Require(! host.DebugHasActiveAnimationSubscription(), "cancelling the last request stops the wakes");
    host.DebugSetAnimationClockForTest(std::nullopt);
}

// One timer per thread: the one-shot timer for the earliest deadline across hosts, the periodic timer only while some
// host needs every frame, and nothing when nothing waits.
void TestFrameScheduleDispatcherArmsOneTimerForTheEarliestDeadline()
{
    auto& dispatcher = AnimationDispatcher::GetInstance();
    AttachedHostWindow first(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    AttachedHostWindow second(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    auto firstRoot    = std::make_unique<DxUi::Panel>();
    auto* firstProbe  = firstRoot->AddChild<ScheduleProbe>();
    auto secondRoot   = std::make_unique<DxUi::Panel>();
    auto* secondProbe = secondRoot->AddChild<ScheduleProbe>();
    first.Host().SetRoot(std::move(firstRoot));
    second.Host().SetRoot(std::move(secondRoot));
    const uint64_t now = dispatcher.GetCurrentTickMs();
    first.Host().RequestAnimationAt(*firstProbe, now + 60'000u);
    second.Host().RequestAnimationAt(*secondProbe, now + 30'000u);
    Require(dispatcher.DebugIsDeadlineTimerArmedForTest() && dispatcher.DebugGetArmedDeadlineTickMsForTest() == now + 30'000u &&
                ! dispatcher.DebugIsPeriodicTimerRunningForTest(),
            "the one-shot timer is armed for the earliest deadline of the thread's hosts");
    second.Host().RequestAnimation(*secondProbe);
    Require(dispatcher.DebugIsPeriodicTimerRunningForTest() && ! dispatcher.DebugIsDeadlineTimerArmedForTest(),
            "a continuous request runs the periodic timer, which also serves the deadlines");
    second.Host().CancelAnimation(*secondProbe);
    Require(! dispatcher.DebugIsPeriodicTimerRunningForTest() && dispatcher.DebugIsDeadlineTimerArmedForTest() &&
                dispatcher.DebugGetArmedDeadlineTickMsForTest() == now + 60'000u,
            "without it the periodic timer stops and the next deadline is armed");
    first.Host().CancelAnimation(*firstProbe);
    Require(! dispatcher.DebugIsPeriodicTimerRunningForTest() && ! dispatcher.DebugIsDeadlineTimerArmedForTest(), "nothing is armed when nothing waits");
}

// The first periodic tick after idle starts the timeline at the real time it runs, not 8 ms later.
void TestFrameScheduleFirstTickAfterIdleAddsNoPhantomFrame()
{
    struct Capture
    {
        uint64_t tickMs   = 0u;
        uint64_t uptimeMs = 0u;
        size_t ticks      = 0u;
    } capture;
    auto& dispatcher        = AnimationDispatcher::GetInstance();
    const auto subscription = dispatcher.Subscribe(
        [](void* context, uint64_t nowTickMs) noexcept
    {
        auto& result = *static_cast<Capture*>(context);
        if (result.ticks++ == 0u)
        {
            result.tickMs   = nowTickMs;
            result.uptimeMs = GetTickCount64();
        }
        return false;
    },
        &capture);
    Require(subscription != 0u && ! dispatcher.DebugIsDeadlineTimerArmedForTest() && dispatcher.DebugIsPeriodicTimerRunningForTest(),
            "a tick subscriber runs the periodic timer");
    Require(PumpUntil(5'000u, [&capture] { return capture.ticks > 0u; }), "the dispatcher ticks");
    Require(capture.tickMs <= capture.uptimeMs, "the first tick after idle reads the real time, with no phantom frame");
}

// Detach and popup-close fuzz: a seeded sequence of adds, removals, moves between parents and hosts, hides and shows,
// focus changes, popup hosts opened and closed (one destroyed from its own control's Tick), and requests of every kind,
// some made or acted on from inside Tick. After every operation and wake no host schedule names a destroyed control.
// Under ASan a stale dereference fails the run.
void TestFrameScheduleLifetimeFuzz()
{
    struct Record
    {
        const void* address = nullptr;
        bool destroyed      = false;
    };
    // Records live in a deque-like stable vector of unique pointers, so a probe's destroyed flag outlives it.
    std::vector<std::unique_ptr<Record>> records;
    records.reserve(4096u);
    std::mt19937 random(0x5EEDu);
    const auto chance = [&random](uint32_t percent) { return std::uniform_int_distribution<uint32_t>(0u, 99u)(random) < percent; };

    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host = window.Host();
    std::optional<AttachedHostWindow> popup;
    auto root                               = std::make_unique<DxUi::Panel>();
    std::array<DxUi::Panel*, 3u> panels     = {root->AddChild<DxUi::Panel>(), root->AddChild<DxUi::Panel>(), root->AddChild<DxUi::Panel>()};
    DxUi::PageHost* const pages             = root->AddChild<DxUi::PageHost>();
    std::array<DxUi::TextField*, 2u> fields = {root->AddChild<DxUi::TextField>(), root->AddChild<DxUi::TextField>()};
    host.SetRoot(std::move(root));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    uint64_t now = 100'000u;
    host.DebugSetAnimationClockForTest(now);

    std::function<void(DxUi::ControlHost&)> act; // one random operation, also run from inside Tick
    const auto makeProbe = [&](DxUi::Panel& parent) -> ScheduleProbe*
    {
        auto* probe = parent.AddChild<ScheduleProbe>();
        if (! probe)
            return nullptr;
        for (auto& record : records)
            if (record->destroyed && record->address == probe)
                record->address = nullptr; // the allocator reused a destroyed probe's address
        records.push_back(std::make_unique<Record>(Record{probe, false}));
        probe->destroyed = &records.back()->destroyed;
        probe->onTick    = [&act, &chance](DxUi::ControlHost& tickHost, uint64_t)
        {
            const bool again = chance(40u);
            if (chance(30u))
                act(tickHost); // may remove this probe, a sibling, the root's children or close the popup host
            return again;
        };
        return probe;
    };
    const auto pickChild = [&](DxUi::Panel& panel) -> std::optional<size_t>
    {
        std::vector<size_t> occupied;
        const auto children = panel.GetChildren();
        for (size_t index = 0u; index < children.size(); ++index)
            if (children[index])
                occupied.push_back(index);
        if (occupied.empty())
            return std::nullopt;
        return occupied[std::uniform_int_distribution<size_t>(0u, occupied.size() - 1u)(random)];
    };
    const auto request = [&](DxUi::ControlHost& target, DxUi::Control& control)
    {
        switch (std::uniform_int_distribution<int>(0, 3)(random))
        {
            case 0: target.RequestAnimation(control); break;
            case 1: target.RequestAnimationAt(control, target.GetAnimationTickMs() + std::uniform_int_distribution<uint64_t>(1u, 900u)(random)); break;
            case 2: target.RequestAnimationAt(control, target.GetAnimationTickMs()); break; // Once
            default: target.CancelAnimation(control); break;
        }
    };
    bool acting = false;
    act         = [&](DxUi::ControlHost& origin)
    {
        if (acting && ! chance(50u))
            return; // bound the recursion of operations started from inside Tick
        const bool wasActing = std::exchange(acting, true);
        const auto restore   = wil::scope_exit([&] { acting = wasActing; });
        DxUi::Panel& panel   = *panels[std::uniform_int_distribution<size_t>(0u, panels.size() - 1u)(random)];
        switch (std::uniform_int_distribution<int>(0, 10)(random))
        {
            case 0:
            case 1:
                if (auto* probe = makeProbe(panel))
                    request(host, *probe);
                break;
            case 2: // remove and destroy
                if (const auto index = pickChild(panel))
                    static_cast<void>(panel.TakeChild(*index));
                break;
            case 3: // re-parent within the host: a page of the PageHost
                if (const auto index = pickChild(panel))
                    if (auto taken = panel.TakeChild(*index))
                        pages->SetPage(std::move(taken));
                break;
            case 4: // move to the popup host, while it may hold a request
                if (popup)
                    if (const auto index = pickChild(panel))
                    {
                        auto* moving = panel.GetChildren()[*index].get();
                        request(host, *moving);
                        if (auto taken = panel.TakeChild(*index))
                        {
                            popup->Host().SetRoot(std::move(taken));
                            if (auto* moved = popup->Host().GetRoot())
                                request(popup->Host(), *moved);
                        }
                    }
                break;
            case 5: // hide or show
                if (const auto index = pickChild(panel))
                    panel.GetChildren()[*index]->SetVisible(! panel.GetChildren()[*index]->IsVisible());
                else
                    panel.SetVisible(! panel.IsVisible());
                break;
            case 6: // focus change: a caret deadline moves between the fields or ends
                host.SetFocusControl(chance(80u) ? fields[std::uniform_int_distribution<size_t>(0u, 1u)(random)] : nullptr, false);
                break;
            case 7: // open a popup host whose controls request ticks
                if (! popup && &origin == &host)
                {
                    popup.emplace(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
                    ShowWindow(popup->Hwnd(), SW_SHOWNOACTIVATE);
                    popup->Host().DebugSetAnimationClockForTest(now);
                    auto popupRoot = std::make_unique<DxUi::Panel>();
                    auto* inside   = makeProbe(*popupRoot);
                    popup->Host().SetRoot(std::move(popupRoot));
                    if (inside)
                    {
                        // Closing its own host from its Tick: the host is destroyed during its wake.
                        inside->onTick = [&popup](DxUi::ControlHost&, uint64_t)
                        {
                            popup.reset();
                            return true;
                        };
                        request(popup->Host(), *inside);
                    }
                }
                break;
            case 8: // close the popup host while its controls hold requests
                if (popup && &origin != &popup->Host())
                    popup.reset();
                break;
            case 9: // requests of every kind on existing controls
                if (const auto index = pickChild(panel))
                    request(host, *panel.GetChildren()[*index]);
                break;
            default: // replace the PageHost's page
                pages->SetPage(chance(50u) ? std::unique_ptr<DxUi::Control>(std::make_unique<DxUi::Panel>()) : nullptr);
                break;
        }
    };

    const auto verify = [&](const char* when)
    {
        for (const auto& record : records)
        {
            if (! record->destroyed || ! record->address)
                continue;
            Require(! host.DebugScheduleHoldsControlForTest(record->address) && (! popup || ! popup->Host().DebugScheduleHoldsControlForTest(record->address)),
                    when);
        }
    };
    for (int step = 0; step < 2000; ++step)
    {
        act(host);
        verify("no schedule names a destroyed control after an operation");
        now += 16u;
        host.DebugSetAnimationClockForTest(now);
        static_cast<void>(host.DebugAnimationTickForTest(now));
        verify("no schedule names a destroyed control after a host wake");
        if (popup)
        {
            popup->Host().DebugSetAnimationClockForTest(now);
            static_cast<void>(popup->Host().DebugAnimationTickForTest(now));
            verify("no schedule names a destroyed control after a popup wake");
        }
    }
    popup.reset();
    host.SetRoot(nullptr);
    Require(host.DebugGetTickRequestCountForTest() == 0u, "replacing the root drops every request");
    host.DebugSetAnimationClockForTest(std::nullopt);
    size_t destroyedCount = 0u;
    for (const auto& record : records)
        destroyedCount += record->destroyed ? 1u : 0u;
    std::cout << "    frame-schedule fuzz: " << records.size() << " probes, " << destroyedCount << " destroyed\n";
}

// G1 on the native host in real time, through FX1's own scene: real dispatcher wakes, not the fixture clock. A WARP
// WindowHost shown without activation and off every monitor pumps its own messages.
void TestFrameScheduleNativeFx1CaretWakesInRealTime()
{
    using namespace PerformanceFixtures;
    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host = window.Host();
    // FX1's motion setting, as its runners apply it (W3.2: a caret blinks only with motion on).
    auto theme          = host.GetTheme();
    theme.reducedMotion = Fx1::kDefinition.reducedMotion;
    host.SetTheme(theme);
    IdlePort port;
    Context context{Host::Window, Fx1::kVariants[0], host, port};
    const std::unique_ptr<Scenario> scene = Fx1::kDefinition.create();
    RequireSucceeded(scene->Build(context), "FX1 builds on the window host");
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    PumpFor(300u); // first frames

    {
        const WorkCounterScope idle;
        PumpFor(2'000u);
        Require(TimerWakes(idle.Delta()) == 0u, "FX1 with nothing armed: 0 timer wakes");
    }

    scene->Step(context, Fx1::kWarm, 0u); // focuses the editable field (no native focus transfer)
    {
        // W3.2: FX1 pins Windows' defaults, so the caret blinks for 5 s (about 10 flips at 530 ms) and then stays shown.
        const WorkCounterScope editable;
        PumpFor(6'000u);
        const WorkCounters delta = editable.Delta();
        std::cout << "    FX1 editable, native, first 6 s real time: " << delta.wakes.deadlineTimer << " deadline wakes, " << delta.wakes.animationTimer
                  << " periodic wakes, " << delta.frame.tickCalls << " Tick calls, " << delta.wakes.paintMessage << " paints\n";
        Require(TimerWakes(delta) <= 20u, "G1: FX1 editable wakes in 10 s <= 20");
        Require(delta.wakes.animationTimer == 0u && delta.frame.tickCalls >= 9u, "the caret blinks through deadlines alone");
        Require(delta.frame.treeVisitsTick == 0u && delta.frame.tickLegacyTreeWalks == 0u, "no wake walks the tree");
        const WorkCounterScope timedOut;
        PumpFor(4'000u);
        std::cout << "    FX1 editable, native, the 4 s after the caret timeout: " << TimerWakes(timedOut.Delta()) << " wakes\n";
        Require(TimerWakes(timedOut.Delta()) == 0u && timedOut.Delta().frame.tickCalls == 0u && host.DebugIsCaretClockVisibleForTest(),
                "W3.2: 0 wakes after the caret timeout, with the caret shown");
    }
    {
        scene->Step(context, Fx1::kReadOnly, 0u); // focuses the read-only field
        const WorkCounterScope readOnly;
        PumpFor(2'000u);
        Require(TimerWakes(readOnly.Delta()) == 0u && host.DebugIsCaretClockVisibleForTest() && host.DebugGetFrameScheduleWakeForTest().kind == 0u,
                "W3.2: a focused read-only field wakes nothing and shows a steady caret");
        host.SetFocusControl(nullptr, false);
        scene->Step(context, Fx1::kWarm, 0u); // the editable field again: its caret restarts
    }
    {
        ShowWindow(window.Hwnd(), SW_SHOWMINNOACTIVE);
        const WorkCounterScope minimized;
        PumpFor(1'500u);
        Require(TimerWakes(minimized.Delta()) == 0u && minimized.Delta().frame.tickCalls == 0u, "0 wakes while minimized");
        const WorkCounterScope restored;
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        const uint64_t shownAt = GetTickCount64();
        Require(host.DebugIsCaretClockVisibleForTest(), "W3.2: on restore the caret is shown at once");
        Require(PumpUntil(2'000u, [&restored] { return restored.Delta().frame.tickCalls > 0u; }), "the restored caret ticks");
        const uint64_t firstFlipMs = GetTickCount64() - shownAt;
        Require(restored.Delta().frame.tickHeldWhileHidden == 0u && restored.Delta().frame.tickCalls == 1u && TimerWakes(restored.Delta()) == 1u,
                "W3.2: the restart on show replaces the flip held while minimized: one wake, one Tick");
        Require(firstFlipMs >= 400u, "W3.2: the phase restarts on show: the first flip comes about one period later, not at once");
    }
    {
        Require(host.SetNativePresentationVisible(false) == S_OK, "hide the native presentation");
        const WorkCounterScope hidden;
        PumpFor(1'500u);
        Require(TimerWakes(hidden.Delta()) == 0u, "0 wakes with the native presentation hidden");
        Require(host.SetNativePresentationVisible(true) == S_OK, "show the native presentation");
    }
    {
        ShowWindow(window.Hwnd(), SW_HIDE);
        const WorkCounterScope hidden;
        PumpFor(1'500u);
        Require(TimerWakes(hidden.Delta()) <= 1u, "a hidden window wakes at most once, to observe it is hidden");
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    }
    host.SetFocusControl(nullptr, false);
    {
        PumpFor(100u);
        const WorkCounterScope blurred;
        PumpFor(1'500u);
        Require(TimerWakes(blurred.Delta()) == 0u, "a blurred caret leaves nothing armed");
    }
}

namespace
{
// A top-level window with no host of its own, off every monitor and never activated: the parent of a child host.
[[nodiscard]] wil::unique_hwnd MakeQuietTopLevel()
{
    wil::unique_hwnd parent(CreateWindowExW(
        WS_EX_NOACTIVATE, L"STATIC", L"frame-schedule parent", WS_OVERLAPPED, -32000, -32000, 360, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
    Require(parent != nullptr, "the parent window is created");
    return parent;
}

// Turns the attached test window into a visible child of `parent`.
void MakeChildOf(AttachedHostWindow& window, HWND parent)
{
    SetWindowLongPtrW(window.Hwnd(), GWL_STYLE, WS_CHILD | WS_VISIBLE);
    Require(SetParent(window.Hwnd(), parent) != nullptr || GetLastError() == ERROR_SUCCESS, "the host window becomes a child");
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 320, 200, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

// A focused caret on `window`, ticking through the real dispatcher (no test clock).
DxUi::TextField* FocusCaret(AttachedHostWindow& window)
{
    auto root   = std::make_unique<DxUi::Panel>();
    auto* field = root->AddChild<DxUi::TextField>(L"caret");
    field->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    window.Host().SetRoot(std::move(root));
    // W3.2: a caret blinks only with motion on, at the user's blink time; both pinned (530 ms, no caret timeout).
    EnableMotionForTest(window.Host());
    window.Host().DebugSetCaretBlinkSettingsForTest(DxUi::ControlHost::CaretBlinkSettingsForTest{530u, 0u});
    window.Host().SetFocusControl(field, false);
    return field;
}
} // namespace

// F1: a host whose window was seen hidden at a wake resumes, without input, when it is visible again, whatever message
// (or none) it received: its own redraw block, a minimized-then-restored top-level, a parent hidden and shown.
void TestFrameScheduleHiddenHostResumesWhenVisibleAgain()
{
    {
        AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        static_cast<void>(FocusCaret(window));
        PumpFor(100u);
        // WM_SETREDRAW FALSE clears WS_VISIBLE; TRUE restores it without WM_SHOWWINDOW.
        SendMessageW(window.Hwnd(), WM_SETREDRAW, FALSE, 0);
        PumpFor(1'200u); // a caret deadline comes due during the block: the wake sees the window hidden
        Require(! window.Host().DebugHasActiveAnimationSubscription(), "a host hidden by its own redraw block stops waking");
        SendMessageW(window.Hwnd(), WM_SETREDRAW, TRUE, 0);
        Require(window.Host().DebugHasActiveAnimationSubscription(), "the end of the redraw block resumes the held schedule");
        Require(window.Host().DebugIsCaretClockVisibleForTest(), "W3.2: the caret is shown at once when the redraw block ends");
        const WorkCounterScope resumed;
        Require(PumpUntil(2'000u, [&resumed] { return resumed.Delta().frame.tickCalls > 0u; }), "the caret blinks again after the redraw block");
    }
    {
        wil::unique_hwnd parent = MakeQuietTopLevel();
        AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
        MakeChildOf(window, parent.get());
        ShowWindow(parent.get(), SW_SHOWNOACTIVATE);
        static_cast<void>(FocusCaret(window));
        PumpFor(100u);
        ShowWindow(parent.get(), SW_SHOWMINNOACTIVE);
        {
            const WorkCounterScope minimized;
            PumpFor(1'500u);
            Require(minimized.Delta().frame.tickCalls == 0u && TimerWakes(minimized.Delta()) <= 2u,
                    "a child host under a minimized top-level ticks nothing and looks at most once a second");
        }
        ShowWindow(parent.get(), SW_SHOWNOACTIVATE);
        Require(window.Host().DebugHasActiveAnimationSubscription(), "the child host still waits to see its top-level restored");
        const WorkCounterScope restored;
        Require(PumpUntil(2'500u, [&restored] { return restored.Delta().frame.tickCalls > 0u; }), "the child's caret blinks again with no input");
        window.Host().SetFocusControl(nullptr, false);
        SetParent(window.Hwnd(), nullptr);
    }
    {
        wil::unique_hwnd parent = MakeQuietTopLevel();
        AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
        MakeChildOf(window, parent.get());
        ShowWindow(parent.get(), SW_SHOWNOACTIVATE);
        static_cast<void>(FocusCaret(window));
        PumpFor(100u);
        ShowWindow(parent.get(), SW_HIDE);
        PumpFor(1'200u);
        ShowWindow(parent.get(), SW_SHOWNOACTIVATE);
        Require(window.Host().DebugHasActiveAnimationSubscription(), "a child host under a hidden parent keeps waiting for it");
        const WorkCounterScope shown;
        Require(PumpUntil(2'500u, [&shown] { return shown.Delta().frame.tickCalls > 0u; }), "the caret blinks again after the parent is shown");
        window.Host().SetFocusControl(nullptr, false);
        SetParent(window.Hwnd(), nullptr);
    }
}

// F2: a control hidden mid-transition keeps its request without ticking or waking, and lands when shown again; a
// control moved to another host mid-transition lands there.
void TestFrameScheduleHiddenControlLandsWhenShown()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host   = window.Host();
    auto root    = std::make_unique<Panel>();
    auto* page   = root->AddChild<Panel>();
    auto* button = page->AddChild<Button>(std::wstring(L"Go"));
    auto* slider = page->AddChild<Slider>();
    auto* graph  = page->AddChild<ThroughputGraph>();
    auto* probe  = page->AddChild<ScheduleProbe>();
    page->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 32.0f));
    slider->SetBounds(D2D1::RectF(0.0f, 40.0f, 220.0f, 80.0f));
    graph->SetBounds(D2D1::RectF(0.0f, 90.0f, 220.0f, 180.0f));
    host.SetRoot(std::move(root));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    uint64_t clock = 50'000u;
    host.DebugSetAnimationClockForTest(clock);
    const auto tickAt = [&](uint64_t at)
    {
        clock = at;
        host.DebugSetAnimationClockForTest(clock);
        static_cast<void>(host.DebugAnimationTickForTest(clock));
    };

    slider->SetValue(20.0);
    const std::array<ThroughputGraphSample, 2u> first{ThroughputGraphSample{.value = 1.0}, ThroughputGraphSample{.value = 2.0}};
    const std::array<ThroughputGraphSample, 2u> second{ThroughputGraphSample{.value = 1.0}, ThroughputGraphSample{.value = 9.0}};
    graph->SetSamples(first);
    tickAt(clock + 1'000u);
    bool handled = false;
    static_cast<void>(host.HandleMessage(window.Hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(20, 12), handled));
    Require(slider->RequestValue(host, 80.0), "a value animation starts");
    graph->SetSamples(second);
    probe->onTick = [](ControlHost&, uint64_t) { return true; };
    host.RequestAnimation(*probe);
    tickAt(clock + 16u);
    Require(slider->DebugGetDisplayedValue() > 20.0 && slider->DebugGetDisplayedValue() < 80.0, "the thumb is in flight");
    const float hoverInFlight = button->DebugGetHoverAnimationProgress();
    Require(hoverInFlight > 0.0f && hoverInFlight < 1.0f && graph->GetDebugState().transitionActive, "the hover and the graph are in flight");

    page->SetVisible(false);
    Require(host.DebugGetFrameScheduleWakeForTest().kind == 0u && ! host.DebugHasActiveAnimationSubscription(),
            "hidden controls hold their requests without waking the host");
    const size_t probeTicks = probe->ticks;
    tickAt(clock + 50u);
    Require(probe->ticks == probeTicks && button->DebugGetHoverAnimationProgress() == hoverInFlight, "a hidden control is not ticked");

    page->SetVisible(true);
    Require(host.DebugGetFrameScheduleWakeForTest().kind != 0u && host.DebugHasActiveAnimationSubscription(), "showing them resumes their requests");
    tickAt(clock + 1'000u);
    tickAt(clock + 16u);
    Require(probe->ticks > probeTicks, "the shown probe ticks again with no further request");
    Require(slider->DebugGetDisplayedValue() == slider->GetValue(), "the thumb lands on its value");
    Require(button->DebugGetHoverAnimationProgress() == 1.0f, "the hover transition completes");
    Require(! graph->GetDebugState().transitionActive, "the graph's transition completes");

    // Moved to another host while animating: the host it joins ticks it, and the transition lands there.
    AttachedHostWindow other(WindowHost::PresentationMode::HwndSwapChain, true);
    ShowWindow(other.Hwnd(), SW_SHOWNOACTIVATE);
    other.Host().DebugSetAnimationClockForTest(clock);
    Require(slider->RequestValue(host, 20.0), "another value animation starts");
    tickAt(clock + 16u);
    Require(slider->DebugGetDisplayedValue() < 80.0 && slider->DebugGetDisplayedValue() > 20.0, "the thumb is in flight again");
    std::unique_ptr<Control> moved;
    for (size_t index = 0u; index < page->GetChildren().size(); ++index)
        if (page->GetChildren()[index].get() == slider)
            moved = page->TakeChild(index);
    Require(moved != nullptr, "the slider leaves its page");
    other.Host().SetRoot(std::move(moved));
    Require(other.Host().DebugGetFrameScheduleWakeForTest().kind != 0u, "the host the slider joins ticks it");
    other.Host().DebugSetAnimationClockForTest(clock + 1'000u);
    static_cast<void>(other.Host().DebugAnimationTickForTest(clock + 1'000u));
    Require(slider->DebugGetDisplayedValue() == slider->GetValue(), "the moved thumb lands on its value");
    host.DebugSetAnimationClockForTest(std::nullopt);
}

// F3: an embedded view whose schedule empties outside a wake (a caret blurred) starts its next animation afresh: the
// application is woken and the first advance does not count the idle gap.
void TestFrameScheduleEmbeddedEmptiedScheduleRebases()
{
    struct Wakes
    {
        size_t count = 0u;
    };
    const auto run = [](bool withCaretFirst, uint64_t gapMs, double& sampled, size_t& wakesOnRequest)
    {
        GraphicsFixture gpu;
        const auto pool = MakeSchedulePool(gpu);
        Wakes wakes;
        DxUi::EmbeddedCallbacks callbacks{};
        callbacks.context            = &wakes;
        callbacks.requestPreparation = [](void* context) noexcept { ++static_cast<Wakes*>(context)->count; };
        DxUi::EmbeddedHost view;
        RequireSucceeded(view.Attach(pool, StoppedTestClock(callbacks)), "attach the rebase view");
        view.Controls().SetTheme(MakeAnimatedTestThemePalette(false));
        auto root    = std::make_unique<DxUi::Panel>();
        auto* field  = root->AddChild<DxUi::TextField>(L"caret");
        auto* slider = root->AddChild<DxUi::Slider>();
        auto* probe  = root->AddChild<ScheduleProbe>();
        field->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 32.0f));
        slider->SetBounds(D2D1::RectF(0.0f, 40.0f, 220.0f, 80.0f));
        slider->SetValue(20.0);
        view.Controls().SetRoot(std::move(root));
        RequireSucceeded(view.Prepare(320u, 120u), "prepare the rebase view");
        uint64_t appClock = 1'000u;
        static_cast<void>(view.AdvanceAnimation(appClock)); // discovery
        if (withCaretFirst)
        {
            view.Controls().SetFocusControl(field, false);
            static_cast<void>(view.AdvanceAnimation(appClock += 16u));
            view.Controls().SetFocusControl(nullptr, false);
            Require(! AnimationRequested(view.GetAnimationRequest()), "the blurred caret leaves nothing scheduled");
        }
        RequireSucceeded(view.Prepare(320u, 120u), "prepare after the blur");
        appClock += gapMs;
        const size_t before = wakes.count;
        view.Controls().RequestAnimation(*probe); // a request that invalidates nothing itself
        wakesOnRequest = wakes.count - before;
        Require(slider->RequestValue(view.Controls(), 80.0), "a value animation starts");
        static_cast<void>(view.AdvanceAnimation(appClock));
        static_cast<void>(view.AdvanceAnimation(appClock + 50u));
        sampled = slider->DebugGetDisplayedValue();
    };
    double withoutGap = 0.0, afterIdle = 0.0;
    size_t wakesFresh = 0u, wakesAfterBlur = 0u;
    run(false, 0u, withoutGap, wakesFresh);
    run(true, 5'000u, afterIdle, wakesAfterBlur);
    Require(wakesAfterBlur == 1u, "after the blur, a request that invalidates nothing still wakes the application");
    Require(afterIdle == withoutGap && afterIdle > 20.0 && afterIdle < 80.0, "the first advance after the idle gap does not count the gap");
}

// F4: requests made before Attach reach the dispatcher once the host has a window.
void TestFrameScheduleRequestsBeforeAttachArmTheDispatcher()
{
    wil::unique_hwnd hwnd(CreateWindowExW(
        WS_EX_NOACTIVATE, L"STATIC", L"frame-schedule attach", WS_POPUP, -32000, -32000, 200, 120, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
    Require(hwnd != nullptr, "the window is created");
    ShowWindow(hwnd.get(), SW_SHOWNOACTIVATE);
    DxUi::WindowHost host;
    auto root     = std::make_unique<DxUi::Panel>();
    auto* probe   = root->AddChild<ScheduleProbe>();
    probe->onTick = [](DxUi::ControlHost&, uint64_t) { return true; };
    host.SetRoot(std::move(root));
    host.RequestAnimation(*probe);
    Require(host.Attach(hwnd.get()), "the host attaches");
    Require(host.DebugHasActiveAnimationSubscription(), "the request made before Attach arms the dispatcher");
    Require(PumpUntil(1'000u, [probe] { return probe->ticks > 0u; }), "and the probe is ticked");
    host.Detach();
}

// F5 and F6: within one wake, a control cancelled by an earlier Tick is not ticked, a control that cancels itself does not
// re-arm, and a control ticked by the compatibility walk is not ticked again for its own request.
void TestFrameScheduleWakeHonoursCancelsAndTicksOnce()
{
    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host   = window.Host();
    auto root    = std::make_unique<DxUi::Panel>();
    auto* first  = root->AddChild<ScheduleProbe>();
    auto* second = root->AddChild<ScheduleProbe>();
    host.SetRoot(std::move(root));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    host.DebugSetAnimationClockForTest(1'000u);
    first->onTick = [second, first](DxUi::ControlHost& owner, uint64_t)
    {
        owner.CancelAnimation(*second);
        owner.CancelAnimation(*first);
        return true;
    };
    second->onTick = [](DxUi::ControlHost&, uint64_t) { return true; };
    host.RequestAnimation(*first);
    host.RequestAnimation(*second);
    static_cast<void>(host.DebugAnimationTickForTest(1'016u));
    Require(first->ticks == 1u && second->ticks == 0u, "a sibling cancelled earlier in the wake is not ticked");
    Require(! host.DebugScheduleHoldsControlForTest(first) && ! host.DebugScheduleHoldsControlForTest(second),
            "a control that cancels itself during its Tick does not re-arm");

    first->onTick = [](DxUi::ControlHost&, uint64_t) { return false; };
    first->ticks  = 0u;
    host.RequestAnimation(*first);
    host.RequestAnimation(); // the compatibility walk in the same wake
    static_cast<void>(host.DebugAnimationTickForTest(1'032u));
    Require(first->ticks == 1u, "one wake ticks a control at most once");
    host.DebugSetAnimationClockForTest(std::nullopt);
}

// F7: schedule calls from another thread are refused and leave every subscription as it was.
void TestFrameScheduleRefusesForeignThreadRequests()
{
    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host  = window.Host();
    auto root   = std::make_unique<DxUi::Panel>();
    auto* probe = root->AddChild<ScheduleProbe>();
    auto* other = root->AddChild<ScheduleProbe>();
    host.SetRoot(std::move(root));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    host.RequestAnimationAt(*probe, host.GetAnimationTickMs() + 60'000u);
    auto& dispatcher           = AnimationDispatcher::GetInstance();
    const size_t subscriptions = dispatcher.DebugGetActiveSubscriptionCountForTest();
    const auto wake            = host.DebugGetFrameScheduleWakeForTest();
    DWORD refusal              = 0u;
    std::thread foreign([&]
    {
        host.CancelAnimation(*probe);
        host.RequestAnimation(*other);
        host.RequestAnimationAt(*other, 0u);
        host.RequestAnimation();
        refusal = GetLastError();
    });
    foreign.join();
    Require(refusal == ERROR_INVALID_THREAD_ID, "a foreign-thread schedule call is refused");
    Require(host.DebugScheduleHoldsControlForTest(probe) && ! host.DebugScheduleHoldsControlForTest(other) && host.DebugGetTickRequestCountForTest() == 1u,
            "the refused calls change no request");
    Require(dispatcher.DebugGetActiveSubscriptionCountForTest() == subscriptions && host.DebugGetFrameScheduleWakeForTest().kind == wake.kind &&
                host.DebugGetFrameScheduleWakeForTest().dueTickMs == wake.dueTickMs && host.DebugHasActiveAnimationSubscription(),
            "and every subscription stays intact");
    host.CancelAnimation(*probe);
}

// F8: one clock for every wake. After a hitch the periodic timeline lags uptime; when the periodic timer stops, the clock
// continues from where the timeline was, so a caret deadline computed on it keeps its phase instead of coming due at once.
void TestFrameScheduleClockContinuesWhenThePeriodicTimerStops()
{
    auto& dispatcher = AnimationDispatcher::GetInstance();
    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    static_cast<void>(FocusCaret(window));
    struct Hitch
    {
        size_t ticks        = 0u;
        uint64_t lastTickMs = 0u;
    } hitch;
    const uint64_t subscription = dispatcher.Subscribe(
        [](void* context, uint64_t nowTickMs) noexcept
    {
        auto& state = *static_cast<Hitch*>(context);
        if (++state.ticks == 2u)
            Sleep(1'000u); // a one-second hitch: the timeline adds only the clamp
        state.lastTickMs = nowTickMs;
        return state.ticks < 5u;
    },
        &hitch);
    Require(subscription != 0u, "a tick subscriber runs the periodic timer");
    Require(PumpUntil(5'000u, [&hitch] { return hitch.ticks >= 5u; }), "the periodic timer ticks past the hitch");
    PumpFor(30u);
    Require(! dispatcher.DebugIsPeriodicTimerRunningForTest(), "the periodic timer stops");
    const uint64_t afterStop = dispatcher.GetCurrentTickMs();
    Require(afterStop >= hitch.lastTickMs && afterStop - hitch.lastTickMs < 200u, "the clock continues from the last periodic tick");
    const auto wake = window.Host().DebugGetFrameScheduleWakeForTest();
    Require(wake.kind == 2u && wake.dueTickMs > afterStop, "the caret's next flip is still ahead: its phase is kept");
    window.Host().SetFocusControl(nullptr, false);
}

// The lifetime fuzz through the real dispatcher: pumped messages, hosts destroyed and recreated, roots replaced, windows
// and presentation hidden and shown and a modal loop, all from inside Tick. No live host's schedule names a destroyed
// control; under ASan any stale dereference fails the run.
void TestFrameScheduleLifetimeFuzzThroughDispatcher()
{
    struct Record
    {
        const void* address = nullptr;
        bool destroyed      = false;
    };
    std::vector<std::unique_ptr<Record>> records;
    std::mt19937 random(0xD15Bu);
    const auto chance = [&random](uint32_t percent) { return std::uniform_int_distribution<uint32_t>(0u, 99u)(random) < percent; };
    std::array<std::optional<AttachedHostWindow>, 2u> hosts;
    std::function<void(size_t)> act;
    bool inModal         = false;
    const auto makeProbe = [&](DxUi::Panel& parent) -> ScheduleProbe*
    {
        auto* probe = parent.AddChild<ScheduleProbe>();
        if (! probe)
            return nullptr;
        for (auto& record : records)
            if (record->destroyed && record->address == probe)
                record->address = nullptr;
        records.push_back(std::make_unique<Record>(Record{probe, false}));
        probe->destroyed = &records.back()->destroyed;
        probe->onTick    = [&act, &chance, &random](DxUi::ControlHost&, uint64_t)
        {
            const bool again = chance(50u);
            if (chance(25u))
                act(std::uniform_int_distribution<size_t>(0u, 1u)(random));
            return again;
        };
        return probe;
    };
    const auto build = [&](size_t which)
    {
        hosts[which].emplace(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
        ShowWindow(hosts[which]->Hwnd(), SW_SHOWNOACTIVATE);
        auto root = std::make_unique<DxUi::Panel>();
        for (int index = 0; index < 4; ++index)
            if (auto* probe = makeProbe(*root))
                hosts[which]->Host().RequestAnimation(*probe);
        hosts[which]->Host().SetRoot(std::move(root));
    };
    act = [&](size_t which)
    {
        if (! hosts[which])
            return;
        auto& host      = hosts[which]->Host();
        auto* rootPanel = dynamic_cast<DxUi::Panel*>(host.GetRoot());
        switch (std::uniform_int_distribution<int>(0, 9)(random))
        {
            case 0: hosts[which].reset(); break; // destroy the host, possibly during its own wake
            case 1:
            {
                auto replacement = std::make_unique<DxUi::Panel>();
                if (auto* probe = makeProbe(*replacement))
                    host.RequestAnimation(*probe);
                host.SetRoot(std::move(replacement)); // a new root during a wake
                break;
            }
            case 2: ShowWindow(hosts[which]->Hwnd(), chance(50u) ? SW_HIDE : SW_SHOWNOACTIVATE); break;
            case 3: static_cast<void>(host.SetNativePresentationVisible(chance(50u))); break;
            case 4:
                if (! inModal)
                {
                    inModal = true;
                    PumpFor(30u); // a modal loop inside Tick: nested wakes are refused
                    inModal = false;
                }
                break;
            case 5:
                if (rootPanel && ! rootPanel->GetChildren().empty())
                {
                    const size_t index = std::uniform_int_distribution<size_t>(0u, rootPanel->GetChildren().size() - 1u)(random);
                    if (rootPanel->GetChildren()[index])
                        rootPanel->GetChildren()[index]->SetVisible(! rootPanel->GetChildren()[index]->IsVisible());
                }
                break;
            case 6:
                if (rootPanel && ! rootPanel->GetChildren().empty())
                    static_cast<void>(rootPanel->TakeChild(std::uniform_int_distribution<size_t>(0u, rootPanel->GetChildren().size() - 1u)(random)));
                break;
            case 7:
                if (rootPanel)
                    if (auto* probe = makeProbe(*rootPanel))
                        host.RequestAnimationAt(*probe, host.GetAnimationTickMs() + std::uniform_int_distribution<uint64_t>(0u, 40u)(random));
                break;
            case 8:
                if (rootPanel && hosts[1u - which] && ! rootPanel->GetChildren().empty() && rootPanel->GetChildren().front())
                {
                    auto taken = rootPanel->TakeChild(0u);
                    if (taken && hosts[1u - which])
                        hosts[1u - which]->Host().SetRoot(std::move(taken)); // move to the other host mid-request
                }
                break;
            default:
                if (rootPanel)
                    host.RequestAnimation();
                break;
        }
    };
    const auto verify = [&]
    {
        for (const auto& record : records)
            if (record->destroyed && record->address)
                for (auto& host : hosts)
                    Require(! host || ! host->Host().DebugScheduleHoldsControlForTest(record->address), "no live schedule names a destroyed control");
    };
    build(0u);
    build(1u);
    for (int round = 0; round < 400; ++round)
    {
        for (size_t which = 0u; which < hosts.size(); ++which)
            if (! hosts[which])
                build(which);
        if (chance(60u))
            act(std::uniform_int_distribution<size_t>(0u, 1u)(random));
        PumpFor(15u);
        verify();
    }
    hosts[0].reset();
    hosts[1].reset();
    verify();
    std::cout << "    frame-schedule dispatcher fuzz: " << records.size() << " probes\n";
}

namespace
{
constexpr wchar_t kThreadExitChildArgument[] = L"--frame-schedule-thread-exit-child";
}

// A dispatcher that a host armed and an orphan subscription kept alive, on a worker thread that ends after the process
// failure host is uninstalled: thread-local destruction of the dispatcher must not report a missing host. Runs in a child process.
namespace
{
constexpr wchar_t kTimerReinstallChildArgument[] = L"--frame-schedule-timer-reinstall-child";
constexpr wchar_t kLateHostChildArgument[]       = L"--frame-schedule-late-host-child";

// Runs this executable with rgument and returns its exit code.
[[nodiscard]] DWORD RunScheduleChild(const wchar_t* argument)
{
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    Require(length > 0u && length < executable.size(), "the test executable path");
    std::wstring command = L"\"" + std::wstring(executable.data(), length) + L"\" " + argument;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    Require(CreateProcessW(executable.data(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info) != FALSE,
            "start the frame-schedule child");
    wil::unique_handle process(info.hProcess), thread(info.hThread);
    Require(WaitForSingleObject(process.get(), 60'000u) == WAIT_OBJECT_0, "the frame-schedule child ends");
    DWORD exitCode = 0u;
    Require(GetExitCodeProcess(process.get(), &exitCode) != FALSE, "the frame-schedule child's exit code");
    return exitCode;
}

[[nodiscard]] HWND FindThreadDispatcherWindow() noexcept
{
    for (HWND window = FindWindowExW(HWND_MESSAGE, nullptr, L"DxUi.AnimationDispatcher", nullptr); window;
         window      = FindWindowExW(HWND_MESSAGE, window, L"DxUi.AnimationDispatcher", nullptr))
        if (GetWindowThreadProcessId(window, nullptr) == GetCurrentThreadId())
            return window;
    return nullptr;
}
} // namespace

// F10: a timer message that arrives while no failure host is installed stops that timer and records it as stopped, so a
// later subscriber arms it again. Runs in a child process.
void TestFrameScheduleTimerRearmsAfterTheFailureHostReturns()
{
    const DWORD exitCode = RunScheduleChild(kTimerReinstallChildArgument);
    std::cout << "    timer-reinstall child exit code 0x" << std::hex << exitCode << std::dec << '\n';
    Require(exitCode == 0u, "the dispatcher records the timer it stopped without a failure host");
}

// F9: a host destroyed after its thread's dispatcher (thread-local destruction order) detaches without touching the
// destroyed dispatcher. Runs in a child process.
void TestFrameScheduleHostOutlivingItsThreadDispatcherDetachesSafely()
{
    const DWORD exitCode = RunScheduleChild(kLateHostChildArgument);
    std::cout << "    late-host child exit code 0x" << std::hex << exitCode << std::dec << '\n';
    Require(exitCode == 0u, "a host destroyed after its thread's dispatcher detaches safely");
}

void TestFrameScheduleDispatcherThreadExitWithoutFailureHost()
{
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    Require(length > 0u && length < executable.size(), "the test executable path");
    std::wstring command = L"\"" + std::wstring(executable.data(), length) + L"\" " + kThreadExitChildArgument;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    Require(CreateProcessW(executable.data(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info) != FALSE,
            "start the thread-exit child");
    wil::unique_handle process(info.hProcess), thread(info.hThread);
    Require(WaitForSingleObject(process.get(), 60'000u) == WAIT_OBJECT_0, "the thread-exit child ends");
    DWORD exitCode = 0u;
    Require(GetExitCodeProcess(process.get(), &exitCode) != FALSE, "the thread-exit child's exit code");
    std::cout << "    thread-exit child exit code 0x" << std::hex << exitCode << std::dec << '\n';
    Require(exitCode == 0u, "a worker thread ending with an armed dispatcher and no failure host exits normally");
}

namespace
{
int RunTimerReinstallChild()
{
    if (FAILED(DxUiFailureTests::ConfigureFailureEvidence()) || FAILED(DxUiFailureTests::InstallFailureHost()))
        return 10;
    auto& dispatcher  = AnimationDispatcher::GetInstance();
    size_t firstTicks = 0u;
    uint64_t first    = 0u;
    {
        const DxUi::Failure::Scope setup;
        first = dispatcher.Subscribe([](void* context, uint64_t) noexcept { return ++*static_cast<size_t*>(context), true; }, &firstTicks);
    }
    if (first == 0u || ! PumpUntil(2'000u, [&firstTicks] { return firstTicks > 0u; }))
        return 11;
    const HWND window = FindThreadDispatcherWindow();
    if (! window || FAILED(DxUiFailureTests::UninstallFailureHost()))
        return 12;
    // A timer message with no failure host: the window procedure stops the timer without entering DxUi work, and records
    // that it is stopped, so the next subscriber (once a failure host is installed again) arms it again.
    SendMessageW(window, WM_TIMER, 1u, 0);
    const bool stopped = ! dispatcher.DebugIsPeriodicTimerRunningForTest();
    std::cout << "DXUI_FRAME_SCHEDULE_TIMER_REINSTALL_CHILD stopped=" << stopped << '\n';
    return stopped ? 0 : 14;
}

// A host whose holder is a thread-local object constructed before the thread's dispatcher: thread-local destruction runs
// the dispatcher's destructor first, then the holder's, which detaches the host with a request still held.
struct LateHost final
{
    LateHost()
    {
        hwnd.reset(CreateWindowExW(WS_EX_NOACTIVATE, L"STATIC", L"late host", WS_POPUP, -32000, -32000, 64, 64, nullptr, nullptr, nullptr, nullptr));
    }
    LateHost(const LateHost&)            = delete;
    LateHost& operator=(const LateHost&) = delete;
    LateHost(LateHost&&)                 = delete;
    LateHost& operator=(LateHost&&)      = delete;
    ~LateHost()
    {
        const DxUi::Failure::Scope teardown;
        host.reset();
        hwnd.reset();
    }
    wil::unique_hwnd hwnd;
    std::unique_ptr<DxUi::WindowHost> host;
};

int RunLateHostChild()
{
    if (FAILED(DxUiFailureTests::ConfigureFailureEvidence()) || FAILED(DxUiFailureTests::InstallFailureHost()))
        return 10;
    int workerResult = 0;
    std::thread worker([&]
    {
        static thread_local std::optional<LateHost> holder; // constructed (empty) before the dispatcher, destroyed after it
        holder.emplace();
        const DxUi::Failure::Scope setup;
        holder->host = std::make_unique<DxUi::WindowHost>();
        if (! holder->hwnd || ! holder->host->Attach(holder->hwnd.get()))
        {
            workerResult = 11;
            return;
        }
        ShowWindow(holder->hwnd.get(), SW_SHOWNOACTIVATE);
        auto root   = std::make_unique<DxUi::Panel>();
        auto* probe = root->AddChild<DxUi::ProgressBar>();
        holder->host->SetRoot(std::move(root));
        holder->host->RequestAnimation(*probe);
        if (! holder->host->DebugHasActiveAnimationSubscription())
            workerResult = 12;
    });
    worker.join();
    std::cout << "DXUI_FRAME_SCHEDULE_LATE_HOST_CHILD worker=" << workerResult << '\n';
    return workerResult;
}
} // namespace

int RunFrameScheduleThreadExitChild(int argc, wchar_t** argv)
{
    if (argc == 2 && std::wstring_view(argv[1]) == kTimerReinstallChildArgument)
        return RunTimerReinstallChild();
    if (argc == 2 && std::wstring_view(argv[1]) == kLateHostChildArgument)
        return RunLateHostChild();
    if (argc != 2 || std::wstring_view(argv[1]) != kThreadExitChildArgument)
        return -1;
    if (FAILED(DxUiFailureTests::ConfigureFailureEvidence()) || FAILED(DxUiFailureTests::InstallFailureHost()))
        return 10;
    wil::unique_event armed(wil::EventOptions::ManualReset);
    wil::unique_event release(wil::EventOptions::ManualReset);
    int workerResult = 0;
    std::thread worker([&]
    {
        {
            const DxUi::Failure::Scope setup;
            // A host that armed the dispatcher and has gone, as an application leaves it, and an orphan subscription that
            // nobody removes: the dispatcher keeps its window and its periodic timer until the thread ends. (A host still
            // attached pins the failure registration, so the host cannot be uninstalled while one lives.)
            wil::unique_hwnd hwnd(
                CreateWindowExW(WS_EX_NOACTIVATE, L"STATIC", L"thread exit", WS_POPUP, -32000, -32000, 64, 64, nullptr, nullptr, nullptr, nullptr));
            {
                DxUi::WindowHost host;
                if (! hwnd || ! host.Attach(hwnd.get()))
                    workerResult = 11;
                auto root   = std::make_unique<DxUi::Panel>();
                auto* probe = root->AddChild<DxUi::ProgressBar>();
                host.SetRoot(std::move(root));
                host.RequestAnimation(*probe);
                static_cast<void>(AnimationDispatcher::GetInstance().Subscribe([](void*, uint64_t) noexcept { return true; }, nullptr));
            }
            if (! AnimationDispatcher::GetInstance().DebugIsPeriodicTimerRunningForTest())
                workerResult = 12;
        }
        armed.SetEvent();
        static_cast<void>(WaitForSingleObject(release.get(), INFINITE));
        // Returns with the dispatcher's window and timer alive; its thread-local destruction follows.
    });
    static_cast<void>(WaitForSingleObject(armed.get(), INFINITE));
    const HRESULT uninstalled = DxUiFailureTests::UninstallFailureHost();
    release.SetEvent();
    worker.join();
    if (FAILED(uninstalled))
        return 13;
    std::cout << "DXUI_FRAME_SCHEDULE_THREAD_EXIT_CHILD worker=" << workerResult << '\n';
    return workerResult;
}
