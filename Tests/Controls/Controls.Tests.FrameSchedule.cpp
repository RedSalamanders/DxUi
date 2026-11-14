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
// deadline ticks nothing before it is due while NeedsAnimation stays true, and no wake walks the tree.
void TestFrameScheduleTicksOnlyDueRequesters()
{
    GraphicsFixture gpu;
    const auto pool = MakeSchedulePool(gpu);
    DxUi::EmbeddedHost view;
    RequireSucceeded(view.Attach(pool), "attach the frame-schedule view");
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
    Require(! view.NeedsAnimation(), "nothing requests after the discovery walk");
    continuous->ticks  = 0u;
    deadline->ticks    = 0u;
    continuous->onTick = [continuous](DxUi::ControlHost&, uint64_t) { return continuous->ticks < 3u; };

    const uint64_t start = view.Controls().GetAnimationTickMs();
    view.Controls().RequestAnimation(*continuous);
    view.Controls().RequestAnimationAt(*deadline, start + 500u);
    Require(view.NeedsAnimation(), "the requests ask for ticks");
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
    Require(view.NeedsAnimation(), "NeedsAnimation stays true while a deadline is pending");
    const uint64_t due = start + 500u - view.Controls().GetAnimationTickMs();
    tick += due - 1u;
    static_cast<void>(view.AdvanceAnimation(tick));
    Require(deadline->ticks == 0u && view.NeedsAnimation(), "one millisecond early ticks nothing");
    tick += 1u;
    {
        const WorkCounterScope wake;
        Require(! view.AdvanceAnimation(tick), "the last request is consumed");
        Require(wake.Delta().frame.tickCalls == 1u && deadline->ticks == 1u, "the due deadline ticks its control once");
    }
    Require(! view.NeedsAnimation(), "nothing is scheduled after the deadline");
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
        RequireSucceeded(view.Attach(pool), "attach the legacy view");
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
        while (view.NeedsAnimation())
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
        const WorkCounterScope editable;
        PumpFor(10'000u);
        const WorkCounters delta = editable.Delta();
        std::cout << "    FX1 editable, native, 10 s real time: " << delta.wakes.deadlineTimer << " deadline wakes, " << delta.wakes.animationTimer
                  << " periodic wakes, " << delta.frame.tickCalls << " Tick calls, " << delta.wakes.paintMessage << " paints\n";
        Require(TimerWakes(delta) <= 20u, "G1: FX1 editable wakes in 10 s <= 20");
        Require(delta.wakes.animationTimer == 0u && delta.frame.tickCalls >= 15u, "the caret blinks through deadlines alone");
        Require(delta.frame.treeVisitsTick == 0u && delta.frame.tickLegacyTreeWalks == 0u, "no wake walks the tree");
    }
    {
        ShowWindow(window.Hwnd(), SW_SHOWMINNOACTIVE);
        const WorkCounterScope minimized;
        PumpFor(1'500u);
        Require(TimerWakes(minimized.Delta()) == 0u && minimized.Delta().frame.tickCalls == 0u, "0 wakes while minimized");
        const WorkCounterScope restored;
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        Require(PumpUntil(2'000u, [&restored] { return restored.Delta().frame.tickCalls > 0u; }), "the restored caret ticks");
        Require(restored.Delta().frame.tickHeldWhileHidden == 1u && restored.Delta().frame.tickCalls == 1u && TimerWakes(restored.Delta()) == 1u,
                "the deadline that expired while minimized fires once, at one wake");
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
constexpr wchar_t kThreadExitChildArgument[] = L"--frame-schedule-thread-exit-child";
}

// A dispatcher that a host armed and an orphan subscription kept alive, on a worker thread that ends after the process
// failure host is uninstalled: thread-local destruction of the dispatcher must not report a missing host. Runs in a child process.
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

int RunFrameScheduleThreadExitChild(int argc, wchar_t** argv)
{
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
