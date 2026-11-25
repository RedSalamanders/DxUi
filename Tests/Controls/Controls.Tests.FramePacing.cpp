#include "Controls.Tests.FramePacing.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.PerformanceFixtures.h"
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // scenarios and ports are never copied
#include "Embedded.Tests.PerformanceFixtureFx1.h"
#include "Embedded.Tests.PerformanceFixtureFx10.h"
#include "Embedded.Tests.PerformanceFixtureFx11.h"
#pragma warning(pop)
#include "GraphicsFixture.h"
#include "Support/AnimationDispatcher.h"
#include "Support/FramePacing.Tests.Hooks.h"
#include "Support/Trace.h"
#include "Support/WorkCounters.h"

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// W3.6's gate, every clause that does not need a display at a chosen refresh rate: on WARP, with the work counters, the trace
// provider and the pacer's test seams. The hardware clause (FX11 at 60 and 144 Hz) is TestFramePacingSampleTimeIsTargetTimeOnHardware.
namespace
{
using DxUi::Detail::WorkCounters;
using DxUi::Detail::WorkCounterScope;
namespace Trace = DxUi::Detail::Trace;

// A control whose Tick runs a test's function and counts its ticks.
class PaceProbe final : public DxUi::Control
{
public:
    PaceProbe()                            = default;
    PaceProbe(const PaceProbe&)            = delete;
    PaceProbe& operator=(const PaceProbe&) = delete;
    PaceProbe(PaceProbe&&)                 = delete;
    PaceProbe& operator=(PaceProbe&&)      = delete;
    ~PaceProbe() override                  = default;
    void Paint(DxUi::ControlHost&) const override
    {
    }
    bool Tick(DxUi::ControlHost& host, uint64_t nowTickMs) override
    {
        ++ticks;
        lastTickMs = nowTickMs;
        return onTick ? onTick(host, nowTickMs) : false;
    }
    // Asks its host for a frame, as a control whose pixels changed does.
    void Repaint(DxUi::ControlHost& host) const
    {
        Invalidate(host);
    }

    size_t ticks        = 0u;
    uint64_t lastTickMs = 0u;
    std::function<bool(DxUi::ControlHost&, uint64_t)> onTick;
};

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
        if (reached())
            return true;
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, static_cast<DWORD>(end - now), QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
    return reached();
}

// A port for building a fixture scene outside its runner: the steps these tests run never use it.
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

[[nodiscard]] uint64_t PacerWakes(const WorkCounters& delta) noexcept
{
    return delta.wakes.pacedFrame + delta.wakes.pacerTimer;
}

[[nodiscard]] uint64_t TimerWakes(const WorkCounters& delta) noexcept
{
    return delta.wakes.animationTimer + delta.wakes.deadlineTimer;
}

// A shown WARP host with a probe, its first frame presented (the swap chain exists and is paced).
struct PacedWindow final
{
    explicit PacedWindow(DxUi::WindowHost::PresentationMode mode = DxUi::WindowHost::PresentationMode::HwndSwapChain) : window(mode, true)
    {
        auto root = std::make_unique<DxUi::Panel>();
        probe     = root->AddChild<PaceProbe>();
        probe->SetBounds(D2D1::RectF(0.0f, 0.0f, 40.0f, 40.0f));
        window.Host().SetRoot(std::move(root));
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        Require(window.Host().RenderInitialFrameForShow(), "the paced host renders its first frame");
        PumpFor(100u);
        Require(window.Host().DebugGetFramePacerStateForTest().paced, "a WARP swap chain is paced: it has a frame-latency waitable object");
    }
    PacedWindow(const PacedWindow&)            = delete;
    PacedWindow& operator=(const PacedWindow&) = delete;
    PacedWindow(PacedWindow&&)                 = delete;
    PacedWindow& operator=(PacedWindow&&)      = delete;
    ~PacedWindow()                             = default;

    [[nodiscard]] DxUi::WindowHost& Host() noexcept
    {
        return window.Host();
    }
    // Leaves the pacer with nothing armed: no animation, no deferred paint.
    void Settle()
    {
        Require(PumpUntil(1'000u,
                          [this]
        {
            const auto state = Host().DebugGetFramePacerStateForTest();
            return ! state.waitArmed && ! state.deferred && ! state.continuous && ! state.skipTimerArmed;
        }),
                "the pacer settles with nothing armed");
    }

    AttachedHostWindow window;
    PaceProbe* probe = nullptr;
};

struct RecordCapture final
{
    std::vector<Trace::Record> records;
    size_t overflow = 0u;
    explicit RecordCapture(size_t capacity)
    {
        records.reserve(capacity);
    }
    static void Sink(void* context, const Trace::Record& record) noexcept
    {
        auto& capture = *static_cast<RecordCapture*>(context);
        if (capture.records.size() == capture.records.capacity())
        {
            ++capture.overflow;
            return;
        }
        capture.records.push_back(record);
    }
};

class ScopedSink final
{
public:
    explicit ScopedSink(RecordCapture& capture) noexcept
    {
        Trace::InstallSinkForTest(&RecordCapture::Sink, &capture);
    }
    ~ScopedSink()
    {
        Trace::InstallSinkForTest(nullptr, nullptr);
    }
    ScopedSink(const ScopedSink&)            = delete;
    ScopedSink& operator=(const ScopedSink&) = delete;
    ScopedSink(ScopedSink&&)                 = delete;
    ScopedSink& operator=(ScopedSink&&)      = delete;
};

[[nodiscard]] uint64_t QpcNow() noexcept
{
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>(now.QuadPart);
}

[[nodiscard]] uint64_t QpcFrequency() noexcept
{
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    return static_cast<uint64_t>(frequency.QuadPart);
}

// Accessibility mutations raised on this thread (each tells UI Automation the window's content changed); prepared content
// published to text services and UI Automation raises one.
[[nodiscard]] uint64_t Mutations() noexcept
{
    return DxUi::Detail::Work().accessibility.mutations;
}
} // namespace

// The spike behind the design, kept as a regression (G of the item's review): a WARP flip-model chain with the frame-latency
// waitable object at latency 1, in both presentation modes, shown and hidden.
void TestFramePacingWaitableSemanticsOnWarp()
{
    wil::com_ptr<ID3D11Device> device;
    RequireSucceeded(
        D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0u, D3D11_SDK_VERSION, device.put(), nullptr, nullptr),
        "a WARP device");
    wil::com_ptr<IDXGIDevice> dxgiDevice = device.query<IDXGIDevice>();
    wil::com_ptr<IDXGIFactory2> factory;
    RequireSucceeded(CreateDXGIFactory2(0u, IID_PPV_ARGS(factory.put())), "a DXGI factory");
    for (const bool composition : {false, true})
        for (const bool hidden : {false, true})
        {
            wil::unique_hwnd window(CreateWindowExW(
                WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC", L"pacing spike", WS_POPUP, -32000, -32000, 160, 100, nullptr, nullptr, nullptr, nullptr));
            Require(window != nullptr, "a spike window");
            if (! hidden)
                ShowWindow(window.get(), SW_SHOWNOACTIVATE);
            DXGI_SWAP_CHAIN_DESC1 desc{};
            desc.Width            = 160u;
            desc.Height           = 100u;
            desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1u;
            desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount      = 2u;
            desc.Scaling          = DXGI_SCALING_STRETCH;
            desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            desc.AlphaMode        = composition ? DXGI_ALPHA_MODE_PREMULTIPLIED : DXGI_ALPHA_MODE_IGNORE;
            desc.Flags            = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
            wil::com_ptr<IDXGISwapChain1> chain;
            wil::com_ptr<IDCompositionDevice> dcomp;
            wil::com_ptr<IDCompositionTarget> target;
            wil::com_ptr<IDCompositionVisual> visual;
            if (composition)
            {
                RequireSucceeded(factory->CreateSwapChainForComposition(device.get(), &desc, nullptr, chain.put()), "a composition chain");
                RequireSucceeded(DCompositionCreateDevice(dxgiDevice.get(), IID_PPV_ARGS(dcomp.put())), "a composition device");
                RequireSucceeded(dcomp->CreateTargetForHwnd(window.get(), TRUE, target.put()), "a composition target");
                RequireSucceeded(dcomp->CreateVisual(visual.put()), "a visual");
                RequireSucceeded(visual->SetContent(chain.get()), "the chain as content");
                RequireSucceeded(target->SetRoot(visual.get()), "the visual as root");
                RequireSucceeded(dcomp->Commit(), "commit");
            }
            else
                RequireSucceeded(factory->CreateSwapChainForHwnd(device.get(), window.get(), &desc, nullptr, nullptr, chain.put()), "a window chain");
            wil::com_ptr<IDXGISwapChain2> chain2 = chain.query<IDXGISwapChain2>();
            RequireSucceeded(chain2->SetMaximumFrameLatency(1u), "latency 1");
            wil::unique_handle waitable(chain2->GetFrameLatencyWaitableObject());
            Require(waitable != nullptr, "the chain has a frame-latency waitable object");
            const auto available = [&waitable]() noexcept
            {
                int units = 0;
                while (units < 16 && WaitForSingleObject(waitable.get(), 0u) == WAIT_OBJECT_0)
                    ++units;
                return units;
            };
            Require(available() == 1, "the object starts with one unit (latency 1)");
            for (int frame = 0; frame < 3; ++frame)
            {
                RequireSucceeded(chain->Present(1u, 0u), "a present");
                Require(WaitForSingleObject(waitable.get(), INFINITE) == WAIT_OBJECT_0, "each retired present returns one unit, shown or hidden");
                Require(WaitForSingleObject(waitable.get(), 0u) == WAIT_TIMEOUT, "and only one");
            }
            // ResizeBuffers while the unit is held: neither adds nor removes one.
            RequireSucceeded(chain->ResizeBuffers(0u, 200u, 120u, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT), "resize");
            Require(WaitForSingleObject(waitable.get(), 0u) == WAIT_TIMEOUT, "ResizeBuffers adds no unit while one is held");
            Require(chain->ResizeBuffers(0u, 210u, 130u, DXGI_FORMAT_UNKNOWN, 0u) == E_INVALIDARG, "ResizeBuffers without the waitable flag is refused");
            RequireSucceeded(chain->Present(1u, 0u), "a present after the resize");
            Require(WaitForSingleObject(waitable.get(), INFINITE) == WAIT_OBJECT_0, "the frame presented after a resize retires");
            // Presents without a unit do not block: they queue, and each returns a unit when it retires. "Does not block" is a claim
            // about the call, not about the machine: a present that blocks for a vertical blank takes that long in every batch, while
            // a busy machine preempts a batch only some of the time. So the claim is judged by the fastest of up to eight batches,
            // each collected before the next so every batch starts from the same state. The minimum is the statistic that load cannot
            // inflate in every batch at once, and that a blocking present cannot get under the bound in any batch.
            uint64_t blockedUs = UINT64_MAX;
            for (int batch = 0; batch < 8 && blockedUs >= 8'000u; ++batch)
            {
                const uint64_t before = QpcNow();
                for (int frame = 0; frame < 3; ++frame)
                    RequireSucceeded(chain->Present(1u, 0u), "an unwaited present");
                blockedUs = std::min(blockedUs, (QpcNow() - before) * 1'000'000u / QpcFrequency());
                // The batch's three units are awaited as a condition (no wall-time bound but the test watchdog); then a zero wait
                // shows there is no fourth.
                for (int unit = 0; unit < 3; ++unit)
                    Require(WaitForSingleObject(waitable.get(), INFINITE) == WAIT_OBJECT_0, "every queued present returns its own unit when it retires");
                Require(WaitForSingleObject(waitable.get(), 0u) == WAIT_TIMEOUT,
                        "every queued present returns its own unit: a present without a credit leaves one unit too many");
            }
            Require(blockedUs < 8'000u, "presents without a unit do not block under latency 1: they queue");
            std::cout << "    [W3.6] spike " << (composition ? "composition" : "hwnd") << (hidden ? " hidden" : " shown")
                      << ": initial 1 unit, 1 per retired present, ResizeBuffers neutral, flags 0 refused, 3 unwaited presents took " << blockedUs << " us\n";
        }
}

// Gate: 0 pacer wakes with nothing armed (FX1 read-only). The editable caret blinks through deadlines and renders each flip
// with a credit taken without waiting, so it costs no pacer wake either.
void TestFramePacingIdleArmsNothing()
{
    using namespace PerformanceFixtures;
    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host          = window.Host();
    auto theme          = host.GetTheme();
    theme.reducedMotion = Fx1::kDefinition.reducedMotion;
    host.SetTheme(theme);
    IdlePort port;
    Context context{Host::Window, Fx1::kVariants[0], host, port};
    const std::unique_ptr<Scenario> scene = Fx1::kDefinition.create();
    RequireSucceeded(scene->Build(context), "FX1 builds on the window host");
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    PumpFor(300u);
    Require(host.DebugGetFramePacerStateForTest().paced, "the FX1 host is paced");

    scene->Step(context, Fx1::kReadOnly, 0u); // focuses the read-only field
    PumpFor(300u);
    {
        const WorkCounterScope readOnly;
        PumpFor(2'000u);
        const WorkCounters delta = readOnly.Delta();
        const auto state         = host.DebugGetFramePacerStateForTest();
        std::cout << "    [W3.6] FX1 read-only, 2 s: " << PacerWakes(delta) << " pacer wakes, " << TimerWakes(delta) << " timer wakes\n";
        Require(PacerWakes(delta) == 0u && TimerWakes(delta) == 0u && ! state.waitArmed && ! state.skipTimerArmed && ! state.continuous,
                "W3.6 gate: 0 pacer wakes with nothing armed (FX1 read-only)");
    }
    host.SetFocusControl(nullptr, false);
    scene->Step(context, Fx1::kWarm, 0u); // the editable field: its caret blinks
    {
        const WorkCounterScope editable;
        PumpFor(3'000u);
        const WorkCounters delta = editable.Delta();
        std::cout << "    [W3.6] FX1 editable, 3 s: " << delta.wakes.deadlineTimer << " deadline wakes, " << PacerWakes(delta) << " pacer wakes, "
                  << delta.frame.presents << " presents, " << delta.frame.pacerDeferredPaints << " deferred paints\n";
        Require(delta.frame.presentsWithoutCredit == 0u && delta.frame.pacerWaitTimeouts == 0u, "every caret frame presents with a credit");
        Require(delta.wakes.animationTimer == 0u && PacerWakes(delta) <= delta.frame.pacerDeferredPaints,
                "a caret wakes the pacer only for a paint it had to defer");
        Require(delta.frame.presents >= 4u, "the caret's flips are presented");
    }
}

// Every way a host is hidden while its wait is armed for an animation: the wait is cancelled and drained before the hide
// returns, and nothing reaches the host afterwards but a stale message, ignored and counted (or, for a window hidden without
// a schedule change, the one frame that observes it hidden, as the frame schedule allows).
void TestFramePacingHideDuringArmedWaitWakesNothing()
{
    enum class Hide
    {
        Presentation,
        Minimize,
        OwnWindow,
    };
    for (const Hide hide : {Hide::Presentation, Hide::Minimize, Hide::OwnWindow})
    {
        PacedWindow paced;
        auto& host          = paced.Host();
        paced.probe->onTick = [](DxUi::ControlHost&, uint64_t) { return true; };
        host.RequestAnimation(*paced.probe);
        Require(PumpUntil(1'000u, [&paced] { return paced.probe->ticks >= 3u; }), "the continuous request ticks through paced frames");
        Require(host.DebugGetFramePacerStateForTest().continuous && host.DebugHasActiveAnimationSubscription(), "the pacer serves the request");
        const size_t ticksBefore = paced.probe->ticks;
        WorkCounterScope afterHide;
        switch (hide)
        {
            case Hide::Presentation: Require(host.SetNativePresentationVisible(false) == S_OK, "hide the presentation"); break;
            case Hide::Minimize: ShowWindow(paced.window.Hwnd(), SW_SHOWMINNOACTIVE); break;
            case Hide::OwnWindow: ShowWindow(paced.window.Hwnd(), SW_HIDE); break;
        }
        if (hide != Hide::OwnWindow)
        {
            const auto state = host.DebugGetFramePacerStateForTest();
            Require(! state.waitArmed && ! state.continuous && ! state.skipTimerArmed && ! state.deferred, "the hide cancels and drains the wait at once");
        }
        PumpFor(500u);
        const WorkCounters delta = afterHide.Delta();
        const uint64_t accepted  = delta.wakes.pacedFrame - delta.frame.pacerStaleFrames + delta.wakes.pacerTimer;
        std::cout << "    [W3.6] hide " << static_cast<int>(hide) << ": " << delta.wakes.pacedFrame << " paced-frame messages (" << delta.frame.pacerStaleFrames
                  << " stale), " << delta.wakes.pacerTimer << " skip timers, " << paced.probe->ticks - ticksBefore << " ticks\n";
        if (hide == Hide::OwnWindow)
            Require(accepted <= 1u && paced.probe->ticks == ticksBefore,
                    "a window hidden without a schedule change is observed by one frame, which ticks nothing");
        else
            Require(accepted == 0u && paced.probe->ticks == ticksBefore && delta.frame.presents == 0u, "0 frames after the hide");
        const auto state = host.DebugGetFramePacerStateForTest();
        Require(! state.waitArmed && ! state.skipTimerArmed && ! state.continuous, "nothing stays armed while hidden");
        switch (hide)
        {
            case Hide::Presentation: Require(host.SetNativePresentationVisible(true) == S_OK, "show the presentation"); break;
            case Hide::Minimize:
            case Hide::OwnWindow: ShowWindow(paced.window.Hwnd(), SW_SHOWNOACTIVATE); break;
        }
        Require(PumpUntil(1'000u, [&] { return paced.probe->ticks > ticksBefore; }), "the request resumes on show");
        host.CancelAnimation(*paced.probe);
        paced.Settle();
    }
}

// Gate: 10 consecutive ticks skipping Present: 0 deadlocks, <= 1 wake per refresh period. A tick that presents nothing keeps
// its credit and ticks again a period later from the skip timer, never from a wait; the tick that invalidates presents with
// that credit, at once.
void TestFramePacingTenSkippedPresentsNeverDeadlock()
{
    for (const auto mode : {DxUi::WindowHost::PresentationMode::HwndSwapChain, DxUi::WindowHost::PresentationMode::CompositionSwapChain})
    {
        PacedWindow paced(mode);
        auto& host = paced.Host();
        paced.Settle();
        constexpr size_t kSkips = 10u;
        paced.probe->onTick     = [&paced](DxUi::ControlHost& tickHost, uint64_t)
        {
            if (paced.probe->ticks == kSkips + 1u)
            {
                paced.probe->Repaint(tickHost);
                return false;
            }
            return true;
        };
        paced.probe->ticks = 0u;
        const WorkCounterScope skipping;
        const uint64_t started = QpcNow();
        host.RequestAnimation(*paced.probe);
        Require(PumpUntil(2'000u, [&paced] { return paced.probe->ticks >= kSkips + 1u; }), "0 deadlocks: ten ticks that skip Present and the next all run");
        const uint64_t elapsedUs = (QpcNow() - started) * 1'000'000u / QpcFrequency();
        Require(PumpUntil(1'000u, [&skipping] { return skipping.Delta().frame.presents >= 1u; }), "the tick that invalidates presents");
        const WorkCounters delta = skipping.Delta();
        // The refresh period the compositor reports (WARP renders to the desktop's display); 16.7 ms if it reports none.
        COMPOSITION_FRAME_ID frameId{};
        COMPOSITION_FRAME_STATS stats{};
        std::array<COMPOSITION_TARGET_ID, 4u> targets{};
        UINT targetCount  = 0u;
        uint64_t periodUs = 16'667u;
        if (SUCCEEDED(DCompositionGetFrameId(COMPOSITION_FRAME_ID_CONFIRMED, &frameId)) &&
            SUCCEEDED(DCompositionGetStatistics(frameId, &stats, static_cast<UINT>(targets.size()), targets.data(), &targetCount)) && stats.framePeriod != 0u)
            periodUs = stats.framePeriod * 1'000'000u / QpcFrequency();
        const uint64_t periods = elapsedUs / periodUs + 1u;
        std::cout << "    [W3.6] ten skipped presents (" << (mode == DxUi::WindowHost::PresentationMode::HwndSwapChain ? "hwnd" : "composition")
                  << "): " << elapsedUs / 1'000u << " ms, " << periods << " refresh periods of " << periodUs << " us, " << delta.wakes.pacedFrame
                  << " paced frames, " << delta.wakes.pacerTimer << " skip timers, " << delta.frame.presents << " presents\n";
        Require(PacerWakes(delta) <= periods + 1u, "W3.6 gate: <= 1 wake per refresh period while ticks skip Present");
        Require(delta.wakes.pacerTimer >= kSkips - 1u, "the skipped ticks come from the skip timer: a held credit is never waited on");
        Require(delta.frame.presentsWithoutCredit == 0u && delta.frame.pacerWaitTimeouts == 0u && delta.frame.presents == 1u,
                "the invalidating tick presents once, with the held credit");
        Require(delta.wakes.animationTimer == 0u, "the dispatcher's periodic timer never runs for a paced host");
        paced.Settle();
    }
}

// Gate: FX10 (live resize) on the window host: 0 ResizeBuffers failures, 0 renders presenting without credit.
void TestFramePacingFx10SweepPresentsOnlyWithCredit()
{
    using namespace PerformanceFixtures;
    const RunRecord run = RunWindowFixture(Fx10::kDefinition, Fx10::kVariants[0]);
    uint64_t presents   = 0u;
    uint64_t without    = 0u;
    uint64_t timeouts   = 0u;
    uint64_t swaps      = 0u;
    for (const PhaseRecord& phase : run.phases)
        for (const StepRecord& step : phase.steps)
        {
            presents += step.work.frame.presents;
            without += step.work.frame.presentsWithoutCredit;
            timeouts += step.work.frame.pacerWaitTimeouts + step.work.frame.pacerSynchronousTimeouts;
            swaps += step.work.frame.swapChains;
        }
    std::cout << "    [W3.6] FX10 window sweep: " << presents << " presents, " << without << " without credit, " << timeouts << " timeouts, " << swaps
              << " swap chains created\n";
    Require(without == 0u && timeouts == 0u, "W3.6 gate: FX10 renders present only with a credit");
    Require(
        swaps == 1u && Summarize(run.Phase("sweep"), "frame.swapChains").total == 0u && Summarize(run.Phase("rest"), "frame.swapChains").total == 0u,
        "W3.6 gate: FX10 creates its chain at the first frame and resizes it at every sweep step: 0 ResizeBuffers failures (a failure recreates the chain)");
    Require(Summarize(run.Phase("sweep"), "frame.renders").minimum == 1u, "every sweep step renders its frame");
}

// Live resize defers, never waits: after each resize the first frame presented has the new size, and no frame is presented at
// a size the window no longer has.
void TestFramePacingResizePresentsOnlyAtTheNewSize()
{
    PacedWindow paced;
    auto& host = paced.Host();
    paced.Settle();
    struct Check
    {
        DxUi::WindowHost* host = nullptr;
        HWND hwnd              = nullptr;
        size_t presents        = 0u;
        size_t stale           = 0u;
    } check{&host, paced.window.Hwnd()};
    const auto sink = [](void* context, const Trace::Record& record) noexcept
    {
        if (record.event != Trace::Event::Present)
            return;
        auto& state = *static_cast<Check*>(context);
        RECT client{};
        GetClientRect(state.hwnd, &client);
        const SIZE chain = state.host->DebugGetSwapChainSizeForTest();
        ++state.presents;
        if (chain.cx != client.right || chain.cy != client.bottom)
            ++state.stale;
    };
    Trace::InstallSinkForTest(sink, &check);
    const auto removeSink = wil::scope_exit([] { Trace::InstallSinkForTest(nullptr, nullptr); });
    const WorkCounterScope resizing;
    for (int step = 0; step < 24; ++step)
    {
        const int width  = 300 + step * 17;
        const int height = 180 + step * 9;
        RECT outer{};
        RECT client{};
        GetWindowRect(paced.window.Hwnd(), &outer);
        GetClientRect(paced.window.Hwnd(), &client);
        SetWindowPos(paced.window.Hwnd(),
                     nullptr,
                     0,
                     0,
                     (outer.right - outer.left) - client.right + width,
                     (outer.bottom - outer.top) - client.bottom + height,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        // A paint comes with the resize, as in a live drag (the window sits off every monitor, so nothing is exposed: the test
        // invalidates it as a drag would): it renders now or defers to the next paced frame.
        const size_t before = check.presents;
        static_cast<void>(InvalidateRect(paced.window.Hwnd(), nullptr, FALSE));
        static_cast<void>(UpdateWindow(paced.window.Hwnd()));
        if (step % 3 == 2)
        {
            const bool presented = PumpUntil(1'000u, [&] { return check.presents > before && ! host.DebugGetFramePacerStateForTest().deferred; });
            if (! presented)
            {
                const auto state = host.DebugGetFramePacerStateForTest();
                const SIZE chain = host.DebugGetSwapChainSizeForTest();
                std::cout << "    [W3.6] resize step " << step << ": presents " << check.presents << " (before " << before << "), credit " << state.credit
                          << ", armed " << state.waitArmed << ", deferred " << state.deferred << ", chain " << chain.cx << 'x' << chain.cy << ", host "
                          << host.DebugGetWidthPx() << 'x' << host.DebugGetHeightPx() << ", renders " << resizing.Delta().frame.renders << '\n';
            }
            Require(presented, "the resized frame is presented by the next paced frame at the latest");
            const SIZE chain = host.DebugGetSwapChainSizeForTest();
            Require(chain.cx == width && chain.cy == height, "the first frame presented after a resize has the new size");
        }
    }
    const WorkCounters delta = resizing.Delta();
    std::cout << "    [W3.6] 24 resizes: " << check.presents << " presents, " << delta.frame.pacerDeferredPaints << " deferred paints, "
              << delta.frame.pacerSynchronousTimeouts << " synchronous timeouts\n";
    Require(check.stale == 0u, "no frame is presented at a stale size");
    Require(delta.frame.presentsWithoutCredit == 0u && delta.frame.pacerSynchronousTimeouts == 0u, "a resize never waits and never presents without credit");
    paced.Settle();
}

// Stale completion: a paced-frame message whose request was cancelled, whose chain was replaced, or which reaches a host
// re-attached to the same HWND, is ignored and counted; none ticks or renders.
void TestFramePacingRejectsStaleFrames()
{
    {
        PacedWindow paced;
        auto& host          = paced.Host();
        paced.probe->onTick = [&paced](DxUi::ControlHost& tickHost, uint64_t)
        {
            paced.probe->Repaint(tickHost);
            return true;
        }; // presents every frame, so the next one waits
        host.RequestAnimation(*paced.probe);
        Require(PumpUntil(1'000u, [&host] { return host.DebugGetFramePacerStateForTest().waitArmed; }), "the wait is armed for the next frame");
        const uint64_t cancelled = host.DebugGetFramePacerStateForTest().armedRequest;
        host.CancelAnimation(*paced.probe);
        Require(! host.DebugGetFramePacerStateForTest().waitArmed, "cancelling the request cancels the wait");
        const size_t ticks = paced.probe->ticks;
        const WorkCounterScope stale;
        host.DebugPostPacedFrameForTest(cancelled);
        PumpFor(150u);
        Require(stale.Delta().frame.pacerStaleFrames >= 1u && stale.Delta().frame.pacedFrames == 0u && paced.probe->ticks == ticks,
                "after cancel: the cancelled request's message is stale");

        host.RequestAnimation(*paced.probe);
        Require(PumpUntil(1'000u, [&host] { return host.DebugGetFramePacerStateForTest().waitArmed; }), "armed again");
        const uint64_t beforeLoss = host.DebugGetFramePacerStateForTest().armedRequest;
        host.DebugSimulateDeviceLoss();
        Require(PumpUntil(2'000u, [&host] { return host.DebugGetFramePacerStateForTest().paced; }), "the chain is recreated and paced again");
        const WorkCounterScope recreated;
        host.DebugPostPacedFrameForTest(beforeLoss);
        PumpFor(150u);
        Require(recreated.Delta().frame.pacerStaleFrames >= 1u, "after chain recreation: the old chain's request is stale");
        host.CancelAnimation(*paced.probe);
        paced.Settle();
        Require(recreated.Delta().frame.pacerTeardownOrderViolations == 0u, "the old chain was released after its wait was drained");
    }
    {
        PacedWindow paced;
        auto& host          = paced.Host();
        paced.probe->onTick = [&paced](DxUi::ControlHost& tickHost, uint64_t)
        {
            paced.probe->Repaint(tickHost);
            return true;
        }; // presents every frame, so the next one waits
        host.RequestAnimation(*paced.probe);
        Require(PumpUntil(1'000u, [&host] { return host.DebugGetFramePacerStateForTest().waitArmed; }), "the wait is armed");
        const uint64_t detached = host.DebugGetFramePacerStateForTest().armedRequest;
        const HWND hwnd         = paced.window.Hwnd();
        host.Detach();
        Require(host.Attach(hwnd, DxUi::WindowHost::AttachOptions{.forceWarp = true}), "re-attach to the same HWND");
        auto root   = std::make_unique<DxUi::Panel>();
        auto* probe = root->AddChild<PaceProbe>();
        host.SetRoot(std::move(root));
        Require(host.RenderInitialFrameForShow(), "the re-attached host renders");
        const WorkCounterScope reattached;
        host.DebugPostPacedFrameForTest(detached);
        PumpFor(150u);
        Require(reattached.Delta().frame.pacerStaleFrames >= 1u && reattached.Delta().frame.pacedFrames == 0u && probe->ticks == 0u,
                "after detach and re-attach on the same HWND: the earlier attachment's request is stale");
    }
}

// Synchronous renders build no latency. A present made without a credit would queue behind the previous frame and later
// return a unit of its own, one more than the host waited for, which raises the queue depth for good; the host never makes
// one. Twenty back-to-back synchronous renders whose callers read the presented result (the bounded-wait path) and twenty
// captures present only with a credit; afterwards the chain holds no spare unit: once a present spends the credit, no unit is
// available until that frame retires (queue depth 1, the bound).
void TestFramePacingSynchronousRendersBuildNoLatency()
{
    PacedWindow paced;
    auto& host = paced.Host();
    paced.Settle();
    const WorkCounterScope synchronous;
    for (int frame = 0; frame < 20; ++frame)
    {
        host.DebugRenderDirtyRectForTest(RECT{0, 0, 40, 40});
        DxUi::WindowHostBitmapCapture bitmap;
        Require(host.DebugCaptureBitmap(bitmap, true), "a capture between synchronous renders");
    }
    const WorkCounters delta = synchronous.Delta();
    std::cout << "    [W3.6] 20 synchronous renders and 20 captures: " << delta.frame.presents << " presents, " << delta.frame.pacerSynchronousTimeouts
              << " bounded-wait timeouts\n";
    Require(delta.frame.presentsWithoutCredit == 0u && delta.frame.presents == 20u - delta.frame.pacerSynchronousTimeouts,
            "every synchronous present spends a credit; captures present nothing");
    PumpFor(200u);
    paced.Settle();
    // Probe the queue: take the credit, spend it on a present, and ask for a unit at once.
    Require(AwaitFrameCreditForTest(host), "a credit after the frames retired");
    host.DebugRenderDirtyRectForTest(RECT{0, 0, 40, 40});
    Require(! host.DebugWaitForFrameCreditForTest(0u), "no spare unit: a frame is in flight and nothing queued beyond it (latency bound 1)");
    Require(AwaitFrameCreditForTest(host), "the in-flight frame returns its unit when it retires");
    host.DebugRenderDirtyRectForTest(RECT{0, 0, 40, 40}); // spends that credit, leaving the pacer as a paint would
    paced.Settle();
}

// A capture-only render neither takes nor needs a credit: it renders and reads the back buffer, presents nothing and leaves
// the pacer as it found it.
void TestFramePacingCaptureNeitherTakesNorNeedsCredit()
{
    PacedWindow paced;
    auto& host = paced.Host();
    paced.Settle();
    // Hold no credit: a test-owned waitable object with no unit, so any paint must defer.
    wil::unique_handle empty(CreateSemaphoreW(nullptr, 0, 8, nullptr));
    host.DebugSetFramePacerWaitableForTest(empty.get());
    const auto before = host.DebugGetFramePacerStateForTest();
    Require(before.paced && ! before.credit, "the pacer holds no credit");
    const WorkCounterScope capture;
    DxUi::WindowHostBitmapCapture bitmap;
    Require(host.DebugCaptureBitmap(bitmap, true), "the capture renders");
    const auto after = host.DebugGetFramePacerStateForTest();
    Require(capture.Delta().frame.presents == 0u && capture.Delta().frame.renders == 1u && ! after.credit && ! after.waitArmed && ! after.deferred,
            "a capture presents nothing, takes no credit and arms nothing");
    host.DebugSetFramePacerWaitableForTest(nullptr);
}

// A stalled compositor (no frame retires: display off, a locked or disconnected session, a remote session that stops
// composing): a pending frame waits, wakes once per timeout and re-arms; it never presents into a full queue and the UI thread
// never blocks. With nothing pending nothing wakes. When frames retire again the deferred frame presents.
void TestFramePacingStalledCompositorDefersWithoutPresenting()
{
    PacedWindow paced;
    auto& host = paced.Host();
    paced.Settle();
    constexpr DWORD kTimeoutMs = 60u;
    wil::unique_handle stalled(CreateSemaphoreW(nullptr, 0, 8, nullptr));
    host.DebugSetFramePacerWaitableForTest(stalled.get(), kTimeoutMs);
    {
        const WorkCounterScope idle;
        PumpFor(400u);
        Require(PacerWakes(idle.Delta()) == 0u && idle.Delta().frame.pacerWaitTimeouts == 0u, "a stall costs nothing while no frame is pending");
    }
    const WorkCounterScope stall;
    paced.probe->Repaint(host);
    const uint64_t started = GetTickCount64();
    PumpFor(600u);
    const uint64_t elapsedMs = GetTickCount64() - started;
    WorkCounters delta       = stall.Delta();
    std::cout << "    [W3.6] stalled compositor, " << elapsedMs << " ms: " << delta.frame.pacerWaitTimeouts << " timeouts, " << delta.wakes.pacedFrame
              << " paced-frame messages, " << delta.frame.presents << " presents, " << delta.frame.renders << " renders\n";
    Require(delta.frame.presents == 0u && delta.frame.presentsWithoutCredit == 0u && delta.frame.renders == 0u,
            "a stalled compositor: nothing is presented into a full queue (the paint stays deferred)");
    Require(delta.frame.pacerWaitTimeouts >= 3u && delta.frame.pacerWaitTimeouts <= elapsedMs / kTimeoutMs + 1u &&
                delta.wakes.pacedFrame == delta.frame.pacerWaitTimeouts,
            "at most one wake per timeout period while the frame is pending");
    Require(host.DebugGetFramePacerStateForTest().deferred && host.DebugGetFramePacerStateForTest().waitArmed,
            "the frame stays deferred and the wait re-armed");
    Require(ReleaseSemaphore(stalled.get(), 1, nullptr) != FALSE, "a frame retires");
    const bool presented = PumpUntil(1'000u, [&stall] { return stall.Delta().frame.presents >= 1u; });
    {
        const auto state = host.DebugGetFramePacerStateForTest();
        delta            = stall.Delta();
        std::cout << "    [W3.6] after the retirement: presents " << delta.frame.presents << ", renders " << delta.frame.renders << ", paced-frame messages "
                  << delta.wakes.pacedFrame << ", stale " << delta.frame.pacerStaleFrames << ", credit " << state.credit << ", armed " << state.waitArmed
                  << ", deferred " << state.deferred << '\n';
    }
    Require(presented && stall.Delta().frame.presents == 1u, "the deferred frame presents once a frame retires");
    delta = stall.Delta();
    Require(delta.frame.presentsWithoutCredit == 0u, "with the credit that retirement returned");
    host.DebugSetFramePacerWaitableForTest(nullptr);
}

// The animation sample of a paced frame is the compositor's first target time after now (within the clock's 1 ms), on the
// thread's one monotonic timeline, with the hitch clamp; a failed statistics call falls back to the current time; a change of
// refresh period is followed at the next frame. Statistics come from the test seam, so this runs on WARP.
void TestFramePacingSampleTimeIsTheTargetTime()
{
    PacedWindow paced;
    auto& host = paced.Host();
    paced.Settle();
    auto& dispatcher         = DxUi::Ui::AnimationDispatcher::GetInstance();
    const uint64_t frequency = QpcFrequency();
    const uint64_t base      = QpcNow();
    uint64_t periodQpc       = frequency / 60u;
    host.DebugSetCompositorStatsForTest(DxUi::WindowHost::CompositorStatsForTest{base, periodQpc, S_OK});
    std::vector<uint64_t> ticks;
    paced.probe->onTick = [&ticks](DxUi::ControlHost&, uint64_t now)
    {
        ticks.push_back(now);
        return ticks.size() < 60u;
    };
    RecordCapture capture(4096u);
    {
        const ScopedSink sink(capture);
        host.RequestAnimation(*paced.probe);
        Require(PumpUntil(3'000u, [&ticks] { return ticks.size() >= 30u; }), "paced frames tick at 60 Hz statistics");
        periodQpc = frequency / 144u;
        host.DebugSetCompositorStatsForTest(DxUi::WindowHost::CompositorStatsForTest{base, periodQpc, S_OK});
        Require(PumpUntil(3'000u, [&ticks] { return ticks.size() >= 60u; }), "and at 144 Hz statistics after a refresh change");
    }
    size_t paced60  = 0u;
    size_t paced144 = 0u;
    size_t exact    = 0u;
    size_t total    = 0u;
    for (const Trace::Record& record : capture.records)
    {
        if (record.event != Trace::Event::FramePaced)
            continue;
        ++total;
        Require(record.detail == static_cast<uint8_t>(Trace::PacedSampleSource::Compositor), "the sample comes from the compositor statistics");
        Require(record.targetQpc > base && (record.targetQpc - base) % record.periodQpc == 0u, "the target is a vertical blank of the reported clock");
        Require(record.targetQpc >= record.qpcTimestamp - record.periodQpc, "the target is the first after now");
        paced60 += record.periodQpc == frequency / 60u ? 1u : 0u;
        paced144 += record.periodQpc == frequency / 144u ? 1u : 0u;
        exact += (! record.clamped && record.sampleQpc == record.targetQpc) ? 1u : 0u;
    }
    for (size_t index = 1u; index < ticks.size(); ++index)
        Require(ticks[index] >= ticks[index - 1u], "the timeline never goes back");
    std::cout << "    [W3.6] fake statistics: " << total << " paced frames (" << paced60 << " at 60 Hz, " << paced144 << " at 144 Hz), " << exact
              << " samples exactly at their target\n";
    Require(paced60 > 0u && paced144 > 0u, "a refresh-rate change is followed at the next frame");
    Require(exact * 100u >= total * 99u, "the sample equals the frame's target time in >= 99% of paced frames");

    // Hitch clamp: a real hitch (the UI thread blocked for 200 ms in a tick) makes the next frame's target 200 ms after the
    // previous sample; the timeline advances by the 50 ms clamp. (A target that far ahead of now is out of range instead:
    // TestFramePacingOutOfRangeTargetDoesNotFreezeTime.)
    ticks.clear();
    paced.probe->onTick = [&ticks](DxUi::ControlHost&, uint64_t now)
    {
        ticks.push_back(now);
        if (ticks.size() == 1u)
            Sleep(200u);
        return ticks.size() < 2u;
    };
    host.DebugSetCompositorStatsForTest(DxUi::WindowHost::CompositorStatsForTest{QpcNow(), frequency / 60u, S_OK});
    RecordCapture clampCapture(64u);
    {
        const ScopedSink sink(clampCapture);
        host.RequestAnimation(*paced.probe);
        Require(PumpUntil(2'000u, [&ticks] { return ticks.size() >= 2u; }), "a frame after a 200 ms hitch");
    }
    bool clamped = false;
    for (const Trace::Record& record : clampCapture.records)
        clamped = clamped || (record.event == Trace::Event::FramePaced && record.clamped);
    Require(clamped, "a step longer than the hitch clamp is clamped, and the trace says so (a miss for the gate)");

    // Fallback: the statistics call fails; the frame samples the current time and is counted.
    host.DebugSetCompositorStatsForTest(DxUi::WindowHost::CompositorStatsForTest{0u, 0u, E_FAIL});
    ticks.clear();
    const WorkCounterScope fallback;
    host.RequestAnimation(*paced.probe);
    Require(PumpUntil(2'000u, [&ticks] { return ticks.size() >= 1u; }), "a frame without statistics");
    Require(fallback.Delta().frame.pacerStatsFallbacks >= 1u, "a failed statistics call falls back to the current time");
    host.CancelAnimation(*paced.probe);
    host.DebugSetCompositorStatsForTest(std::nullopt);
    paced.Settle();
    Require(dispatcher.DebugGetPacedTimelineCountForTest() == 0u, "no paced timeline outlives its animation");
    const uint64_t after = dispatcher.GetCurrentTickMs();
    Require(after >= ticks.back(), "the idle clock continues from the last paced sample, never before it");
}

// The embedded host is unchanged: no swap chain, so no pacer, no wait and no paced wake; it advances on the application's clock.
void TestFramePacingEmbeddedHostOwnsNoPacer()
{
    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "create the embedded WARP device");
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    RequireSucceeded(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "share the embedded graphics");
    DxUi::EmbeddedHost view;
    RequireSucceeded(view.Attach(graphics, StoppedTestClock()), "attach the embedded view");
    auto root     = std::make_unique<DxUi::Panel>();
    auto* probe   = root->AddChild<PaceProbe>();
    probe->onTick = [probe](DxUi::ControlHost&, uint64_t) { return probe->ticks < 5u; };
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(view.Prepare(120u, 80u), "prepare the embedded view");
    const WorkCounterScope embedded;
    view.Controls().RequestAnimation(*probe);
    uint64_t now = 1'000u;
    while (AnimationRequested(view.GetAnimationRequest()))
        static_cast<void>(view.AdvanceAnimation(now += 16u));
    PumpFor(100u);
    const auto state = view.Controls().DebugGetFramePacerStateForTest();
    Require(! state.paced && ! state.waitArmed && PacerWakes(embedded.Delta()) == 0u && probe->ticks >= 5u,
            "an embedded host owns no pacer: it ticks on the application's clock only");
}

// Detach fuzz (run under ASan): hosts animating through paced frames are detached, destroyed, hidden or lose their device at
// random moments while pool callbacks are in flight (a 1 ms wait timeout keeps them coming). No callback touches a destroyed
// host, the teardown order holds, and no stale message is accepted.
void TestFramePacingDetachFuzz()
{
    std::mt19937 random(0x36F5u);
    const auto pick = [&random](uint32_t limit) { return std::uniform_int_distribution<uint32_t>(0u, limit - 1u)(random); };
    const WorkCounterScope fuzz;
    for (int round = 0; round < 40; ++round)
    {
        auto paced           = std::make_unique<PacedWindow>(pick(2u) == 0u ? DxUi::WindowHost::PresentationMode::HwndSwapChain
                                                                            : DxUi::WindowHost::PresentationMode::CompositionSwapChain);
        auto& host           = paced->Host();
        paced->probe->onTick = [](DxUi::ControlHost&, uint64_t) { return true; };
        host.RequestAnimation(*paced->probe);
        if (pick(3u) == 0u)
        {
            // A stalled test object with a 1 ms timeout: callbacks arrive continuously.
            wil::unique_handle stalled(CreateSemaphoreW(nullptr, 0, 4, nullptr));
            host.DebugSetFramePacerWaitableForTest(stalled.get(), 1u);
        }
        PumpFor(pick(30u));
        switch (pick(5u))
        {
            case 0u: host.Detach(); break;
            case 1u: DestroyWindow(paced->window.Hwnd()); break;
            case 2u: static_cast<void>(host.SetNativePresentationVisible(false)); break;
            case 3u: host.DebugSimulateDeviceLoss(); break;
            default: ShowWindow(paced->window.Hwnd(), SW_HIDE); break;
        }
        PumpFor(pick(10u));
        if (pick(2u) == 0u)
            paced.reset(); // destroyed with whatever is still queued for it
        PumpFor(5u);
    }
    PumpFor(100u);
    Require(fuzz.Delta().frame.pacerTeardownOrderViolations == 0u, "every chain was released after its wait was drained and its handle closed");
    std::cout << "    [W3.6] detach fuzz: " << fuzz.Delta().wakes.pacedFrame << " paced-frame messages, " << fuzz.Delta().frame.pacerStaleFrames << " stale\n";
}

// Hardware only: FX11's transitions animate through paced frames on the default hardware adapter, and every paced frame's
// sample is compared with its compositor target (the W0.8 trace's FramePaced events). Gate (read by the conductor): the sample
// equals the target within the clock's 1 ms in >= 99% of paced frames at 60 and at 144 Hz; a fallback or clamped frame is a
// miss. The share of frames whose target was the vertical blank DXGI reports the present displayed at is recorded beside it as
// evidence. Writes .build/w36/fx11-<refresh>hz.json; on WARP or without a hardware adapter it reports a capability skip.
void TestFramePacingSampleTimeIsTargetTimeOnHardware()
{
    using namespace PerformanceFixtures;
    wil::com_ptr<IDXGIFactory1> factory;
    wil::com_ptr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 description{};
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.put()))) || FAILED(factory->EnumAdapters1(0u, adapter.put())) ||
        FAILED(adapter->GetDesc1(&description)) || (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u)
    {
        std::cout << "    [CAPABILITY SKIP] W3.6 hardware pacing: no hardware adapter\n";
        return;
    }
    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, false);
    auto& host = window.Host();
    IdlePort port;
    auto theme          = DxUi::MakeDefaultThemePalette(true);
    theme.reducedMotion = false;
    host.SetTheme(theme);
    const Variant& variant = Fx11::kVariants[0];
    RECT outer{};
    RECT client{};
    GetWindowRect(window.Hwnd(), &outer);
    GetClientRect(window.Hwnd(), &client);
    SetWindowPos(window.Hwnd(),
                 nullptr,
                 0,
                 0,
                 (outer.right - outer.left) - client.right + static_cast<LONG>(variant.width),
                 (outer.bottom - outer.top) - client.bottom + static_cast<LONG>(variant.height),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    Context context{Host::Window, variant, host, port};
    const std::unique_ptr<Scenario> scene = Fx11::kDefinition.create();
    RequireSucceeded(scene->Build(context), "FX11 builds on the hardware window host");
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    PumpFor(300u);
    if (! host.DebugGetFramePacerStateForTest().paced)
    {
        std::cout << "    [CAPABILITY SKIP] W3.6 hardware pacing: the hardware chain offers no frame-latency waitable object\n";
        return;
    }
    // Beside the trace: each present's count with the target its frame was sampled for, and the displayed vertical blank DXGI
    // reports for the latest displayed present.
    struct Displayed
    {
        DxUi::WindowHost* host = nullptr;
        RecordCapture* records = nullptr;
        uint64_t lastTarget    = 0u;
        std::vector<std::pair<UINT, uint64_t>> targets;   // present count, predicted target
        std::vector<std::pair<UINT, uint64_t>> displayed; // present count, SyncQPCTime
    } displayedState{&host};
    RecordCapture capture(1u << 16u);
    displayedState.records = &capture;
    displayedState.targets.reserve(1u << 14u);
    displayedState.displayed.reserve(1u << 14u);
    const auto displayedSink = [](void* context, const Trace::Record& record) noexcept
    {
        auto& state = *static_cast<Displayed*>(context);
        RecordCapture::Sink(state.records, record);
        if (record.event == Trace::Event::FramePaced)
            state.lastTarget = record.targetQpc;
        if (record.event != Trace::Event::Present || state.targets.size() == state.targets.capacity())
            return;
        UINT count = 0u;
        DXGI_FRAME_STATISTICS statistics{};
        if (state.host->DebugGetPresentStatisticsForTest(count, statistics))
        {
            state.targets.emplace_back(count, state.lastTarget);
            if (statistics.PresentCount != 0u && state.displayed.size() < state.displayed.capacity())
                state.displayed.emplace_back(statistics.PresentCount, static_cast<uint64_t>(statistics.SyncQPCTime.QuadPart));
        }
    };
    {
        Trace::InstallSinkForTest(displayedSink, &displayedState);
        const auto removeSink = wil::scope_exit([] { Trace::InstallSinkForTest(nullptr, nullptr); });
        // The 60 Hz rate block's state changes (step 0 of each transition phase) one after another, each left to animate on the
        // display for its window: the host's own clock, paced by the swap chain.
        const size_t first = 1u + Fx11::kTransitions; // the 60 Hz block
        for (int round = 0; round < 4; ++round)
            for (size_t transition = 0u; transition < Fx11::kTransitions; ++transition)
            {
                scene->Step(context, first + transition, 0u);
                PumpFor(Fx11::kWindowMs);
            }
    }
    size_t total     = 0u;
    size_t matched   = 0u;
    size_t clamped   = 0u; // misses: the hitch clamp shortened the step (a frame late by more than 50 ms, or a held clock)
    size_t fallbacks = 0u; // misses: no compositor statistics
    uint64_t period  = 0u;
    for (const Trace::Record& record : capture.records)
    {
        if (record.event != Trace::Event::FramePaced)
            continue;
        ++total;
        period                    = record.periodQpc != 0u ? record.periodQpc : period;
        const uint64_t frequency  = QpcFrequency();
        const uint64_t difference = record.sampleQpc > record.targetQpc ? record.sampleQpc - record.targetQpc : record.targetQpc - record.sampleQpc;
        fallbacks += record.detail != static_cast<uint8_t>(Trace::PacedSampleSource::Compositor) ? 1u : 0u;
        clamped += record.clamped ? 1u : 0u;
        if (record.detail == static_cast<uint8_t>(Trace::PacedSampleSource::Compositor) && ! record.clamped && record.targetQpc != 0u &&
            difference * 1'000u < frequency)
            ++matched;
    }
    // Evidence, not the gate: of the presents DXGI later reported displayed, those whose predicted target was that vertical blank.
    size_t displayedKnown    = 0u;
    size_t displayedAtTarget = 0u;
    // How many refresh periods after (positive) or before its predicted target each displayed present was shown, -3 to +4.
    std::array<size_t, 8u> offsets{};
    for (const auto& [count, syncQpc] : displayedState.displayed)
        for (const auto& [presentCount, target] : displayedState.targets)
            if (presentCount == count && target != 0u)
            {
                ++displayedKnown;
                const uint64_t difference = syncQpc > target ? syncQpc - target : target - syncQpc;
                displayedAtTarget += (period != 0u && difference * 2u < period) ? 1u : 0u;
                if (period != 0u)
                {
                    const auto periods = static_cast<int64_t>((difference + period / 2u) / period) * (syncQpc >= target ? 1 : -1);
                    offsets[static_cast<size_t>(std::clamp<int64_t>(periods, -3, 4) + 3)] += 1u;
                }
                break;
            }
    const uint64_t hz = period != 0u ? (QpcFrequency() + period / 2u) / period : 0u;
    std::filesystem::create_directories(".build/w36");
    const std::string path = ".build/w36/fx11-" + std::to_string(hz) + "hz.json";
    std::ofstream output(path, std::ios::binary);
    output << "{\"item\":\"W3.6\",\"fixture\":\"FX11\",\"adapter\":\"hardware\",\"refreshHz\":" << hz << ",\"pacedFrames\":" << total
           << ",\"sampleAtTarget\":" << matched << ",\"missesClamped\":" << clamped << ",\"missesFallback\":" << fallbacks
           << ",\"displayedPresentsMatched\":" << displayedKnown << ",\"displayedAtPredictedVblank\":" << displayedAtTarget
           << ",\"displayedPeriodsAfterTarget\":{";
    for (size_t index = 0u; index < offsets.size(); ++index)
        output << (index ? "," : "") << '"' << static_cast<int>(index) - 3 << "\":" << offsets[index];
    output << "},\"overflow\":" << capture.overflow << "}\n";
    std::cout << "    [W3.6] hardware FX11 at " << hz << " Hz: " << matched << " of " << total << " paced frames sample their target (" << clamped
              << " clamped, " << fallbacks << " without statistics); " << displayedAtTarget << " of " << displayedKnown
              << " displayed presents at their predicted vertical blank (" << path << ")\n";
    Require(total >= 100u, "FX11 animates through paced frames on hardware");
    // The gate is a timing verdict: it is judged in the hardware slot (A6), on a machine not loaded by other builds, with
    // DXUI_W36_HARDWARE_GATE=1. An ordinary suite run on a shared machine records the evidence only.
    std::array<wchar_t, 8u> gate{};
    if (GetEnvironmentVariableW(L"DXUI_W36_HARDWARE_GATE", gate.data(), static_cast<DWORD>(gate.size())) == 0u || gate[0] != L'1')
    {
        std::cout << "    [EVIDENCE ONLY] W3.6 hardware gate not judged: set DXUI_W36_HARDWARE_GATE=1 in the hardware slot\n";
        return;
    }
    Require(matched * 100u >= total * 99u, "W3.6 gate: the animation sample equals the frame's targetTime in >= 99% of paced frames");
}

// A credit is spent by a frame that was queued, never by an attempt (review A1). A Present that fails with an error that is
// not a device loss (here DXGI_ERROR_INVALID_CALL from a dirty rectangle beyond the buffer) queues no frame, so no unit will
// come back for it: the host keeps its credit and presents the next requested frame, without waiting for a resize. The
// swap chain's present count shows that the failed call queued nothing. The same holds while the host animates through
// paced frames: a failed present there neither stops the ticks nor the presents that follow.
void TestFramePacingFailedPresentKeepsTheCredit()
{
    {
        PacedWindow paced;
        auto& host = paced.Host();
        paced.Settle();
        Require(AwaitFrameCreditForTest(host), "a credit before the failing present");
        UINT countBefore = 0u;
        UINT countAfter  = 0u;
        DXGI_FRAME_STATISTICS displayed{};
        static_cast<void>(host.DebugGetPresentStatisticsForTest(countBefore, displayed));
        const auto width = static_cast<LONG>(host.DebugGetWidthPx());
        host.DebugRenderDirtyRectForTest(RECT{width - 10, 0, width + 50, 20});
        const HRESULT failed = host.DebugGetLastPresentResult();
        static_cast<void>(host.DebugGetPresentStatisticsForTest(countAfter, displayed));
        const auto state = host.DebugGetFramePacerStateForTest();
        std::cout << "    [W3.6] out-of-buffer dirty rectangle: Present1 0x" << std::hex << static_cast<unsigned long>(failed) << std::dec << ", present count "
                  << countBefore << " -> " << countAfter << ", credit " << state.credit << ", armed " << state.waitArmed << '\n';
        Require(failed == DXGI_ERROR_INVALID_CALL, "precondition: the present failed with an error that is not a device loss");
        Require(countAfter == countBefore, "the failed call queued no frame");
        Require(state.credit && ! state.waitArmed, "a call that queued no frame leaves the credit held and nothing waiting");
        const WorkCounterScope after;
        paced.probe->Repaint(host);
        const bool presented = PumpUntil(10'000u, [&after] { return after.Delta().frame.presents >= 1u; });
        std::cout << "    [W3.6] after the failed present: presents " << after.Delta().frame.presents << ", wait timeouts "
                  << after.Delta().frame.pacerWaitTimeouts << ", deferred paints " << after.Delta().frame.pacerDeferredPaints << '\n';
        Require(presented && after.Delta().frame.presentsWithoutCredit == 0u, "the next requested frame presents, with the credit the failure kept");
        Require(after.Delta().frame.pacerDeferredPaints == 0u, "and at once: it finds the credit, nothing waits for a unit");
        Require(after.Delta().frame.pacerInvariantViolations == 0u, "every deferred paint keeps its wake");
        paced.Settle();
    }
    {
        PacedWindow paced;
        auto& host          = paced.Host();
        paced.probe->onTick = [&paced](DxUi::ControlHost& tickHost, uint64_t)
        {
            paced.probe->Repaint(tickHost);
            return true;
        };
        host.RequestAnimation(*paced.probe);
        Require(PumpUntil(10'000u, [&paced] { return paced.probe->ticks >= 3u; }), "the animation ticks through paced frames");
        Require(AwaitFrameCreditForTest(host), "a credit for the failing present");
        const auto width = static_cast<LONG>(host.DebugGetWidthPx());
        host.DebugRenderDirtyRectForTest(RECT{width - 10, 0, width + 50, 20});
        Require(FAILED(host.DebugGetLastPresentResult()), "precondition: the present failed");
        const size_t ticks = paced.probe->ticks;
        const WorkCounterScope after;
        const bool resumed = PumpUntil(10'000u, [&] { return after.Delta().frame.presents >= 5u && paced.probe->ticks >= ticks + 5u; });
        std::cout << "    [W3.6] animating, after a failed present: " << paced.probe->ticks - ticks << " ticks, " << after.Delta().frame.presents
                  << " presents, " << after.Delta().frame.pacerWaitTimeouts << " wait timeouts\n";
        // A lost credit would stop the presents for good; a slow wait under load (a timeout) is not a defect.
        Require(resumed && after.Delta().frame.presentsWithoutCredit == 0u, "an animating host keeps ticking and presenting after a failed present");
        host.CancelAnimation(*paced.probe);
        paced.Settle();
    }
}

// Only S_OK spends the credit (round 2, decision 1): a success status such as DXGI_STATUS_OCCLUDED may or may not have queued a
// frame, and a credit spent on a frame that returns no unit would leave the window never painting again. Through the private
// seam that makes a present report another result (Support/FramePacing.Tests.Hooks.h):
// - a test-owned object that never returns a unit (the case a spent credit could not survive): the host keeps the credit and
//   presents the next requested frame at once;
// - the real chain, whose frame did queue and returns its unit: the host keeps one credit (never more), presents on, and the
//   queue is back within its bound after the next present.
void TestFramePacingSuccessStatusKeepsTheCredit()
{
    {
        PacedWindow paced;
        auto& host = paced.Host();
        paced.Settle();
        wil::unique_handle units(CreateSemaphoreW(nullptr, 0, 8, nullptr));
        host.DebugSetFramePacerWaitableForTest(units.get(), 5'000u);
        Require(ReleaseSemaphore(units.get(), 1, nullptr) != FALSE, "one unit for the occluded present");
        Require(host.DebugWaitForFrameCreditForTest(100u), "the host takes it");
        DxUi::Detail::SetNextPresentResultForTest(DXGI_STATUS_OCCLUDED);
        host.DebugRenderDirtyRectForTest(RECT{0, 0, 40, 40});
        const auto state = host.DebugGetFramePacerStateForTest();
        std::cout << "    [W3.6] present reporting DXGI_STATUS_OCCLUDED: last result 0x" << std::hex
                  << static_cast<unsigned long>(host.DebugGetLastPresentResult()) << std::dec << ", credit " << state.credit << ", armed " << state.waitArmed
                  << '\n';
        Require(host.DebugGetLastPresentResult() == DXGI_STATUS_OCCLUDED, "precondition: the present reported DXGI_STATUS_OCCLUDED");
        Require(state.credit && ! state.waitArmed, "a success status other than S_OK keeps the credit and waits for nothing");
        const WorkCounterScope after;
        paced.probe->Repaint(host);
        const bool presented = PumpUntil(10'000u, [&after] { return after.Delta().frame.presents >= 1u; });
        std::cout << "    [W3.6] next frame with no unit returned: presents " << after.Delta().frame.presents << ", deferred paints "
                  << after.Delta().frame.pacerDeferredPaints << '\n';
        host.DebugSetFramePacerWaitableForTest(nullptr);
        Require(presented && after.Delta().frame.pacerDeferredPaints == 0u && after.Delta().frame.presentsWithoutCredit == 0u,
                "the window presents its next frame at once although no unit came back");
    }
    {
        PacedWindow paced;
        auto& host = paced.Host();
        paced.Settle();
        Require(AwaitFrameCreditForTest(host), "a credit on the real chain");
        DxUi::Detail::SetNextPresentResultForTest(DXGI_STATUS_OCCLUDED);
        host.DebugRenderDirtyRectForTest(RECT{0, 0, 40, 40}); // queued for real, reported as occluded
        Require(host.DebugGetFramePacerStateForTest().credit, "the credit is kept");
        // The occluded frame retires and returns a unit the host's credit already stands for: one too many. Whenever it comes
        // back (the next present or a later one, however loaded the machine), a present made while the credit is held absorbs it.
        const WorkCounterScope next;
        size_t presents = 0u;
        while (next.Delta().frame.pacerExcessUnits == 0u && presents < 50u)
        {
            PumpFor(10u);
            Require(AwaitFrameCreditForTest(host), "a credit for the next present");
            host.DebugRenderDirtyRectForTest(RECT{0, 0, 40, 40});
            ++presents;
        }
        const bool beyondBound = host.DebugWaitForFrameCreditForTest(0u);
        std::cout << "    [W3.6] real chain after an occluded present: " << next.Delta().frame.pacerExcessUnits << " excess unit absorbed after " << presents
                  << " present(s), a unit beyond the bound while the next frame is in flight " << beyondBound << '\n';
        Require(next.Delta().frame.pacerExcessUnits == 1u, "the unit the occluded frame returned is absorbed, once");
        Require(! beyondBound, "the queue is back within its bound: no unit while a frame is in flight");
        PumpFor(150u);
        Require(AwaitFrameCreditForTest(host), "the in-flight frame returns its unit");
        host.DebugRenderDirtyRectForTest(RECT{0, 0, 40, 40});
        paced.Settle();
    }
}

// The hitch clamp keeps its meaning for a real hitch, and a target the compositor could not have meant is not accepted
// (review E1). A sampled target is valid only after now and at most kFramePacerMaxTargetPeriods refresh periods ahead; one
// beyond that (here 3 s ahead, a bad statistics read) samples the current time and is counted, so the timeline keeps
// advancing at real time afterwards and its anchor never lies further ahead of real time than that bound.
void TestFramePacingOutOfRangeTargetDoesNotFreezeTime()
{
    PacedWindow paced;
    auto& host = paced.Host();
    paced.Settle();
    std::vector<uint64_t> ticks;
    ticks.reserve(4096u);
    paced.probe->onTick = [&ticks](DxUi::ControlHost&, uint64_t now)
    {
        ticks.push_back(now);
        return true;
    };
    host.RequestAnimation(*paced.probe);
    Require(PumpUntil(10'000u, [&ticks] { return ticks.size() >= 5u; }), "paced frames tick");
    const uint64_t frequency = QpcFrequency();
    RecordCapture capture(64u);
    const WorkCounterScope outOfRange;
    {
        const ScopedSink sink(capture);
        host.DebugSetCompositorStatsForTest(DxUi::WindowHost::CompositorStatsForTest{QpcNow() + 3u * frequency, frequency / 60u, S_OK});
        const size_t before = ticks.size();
        Require(PumpUntil(10'000u, [&] { return ticks.size() >= before + 1u; }), "a frame samples the out-of-range target");
        host.DebugSetCompositorStatsForTest(std::nullopt);
    }
    bool fellBack = false;
    for (const Trace::Record& record : capture.records)
        fellBack = fellBack || (record.event == Trace::Event::FramePaced && record.detail == static_cast<uint8_t>(Trace::PacedSampleSource::Fallback));
    const uint64_t sampled   = ticks.back();
    const uint64_t realStart = GetTickCount64();
    PumpFor(1'000u);
    const uint64_t realMs     = GetTickCount64() - realStart;
    const uint64_t advancedMs = ticks.back() - sampled;
    std::cout << "    [W3.6] one target 3 s ahead: " << outOfRange.Delta().frame.pacerTargetsOutOfRange << " rejected; then " << realMs
              << " ms of real time advanced the timeline " << advancedMs << " ms\n";
    host.CancelAnimation(*paced.probe);
    paced.Settle();
    Require(outOfRange.Delta().frame.pacerTargetsOutOfRange >= 1u && fellBack, "a target beyond the bound samples the current time and is counted");
    // Frozen, the timeline advanced 0 ms; a loaded machine may lose time to the hitch clamp, never all of it.
    Require(advancedMs * 4u >= realMs, "the timeline keeps advancing after an out-of-range target");
}

// The test seam DebugWaitForFrameCreditForTest cancels the armed wait and takes the credit (review G1): a paint deferred before
// it still renders. The pacer's invariant, checked after each of its transitions in Debug (frame.pacerInvariantViolations): a
// deferred paint always has a wake (an armed wait whose message may already be posted, or the paced frame rendering it now).
void TestFramePacingCreditSeamKeepsDeferredPaint()
{
    PacedWindow paced;
    auto& host = paced.Host();
    paced.Settle();
    wil::unique_handle units(CreateSemaphoreW(nullptr, 0, 8, nullptr));
    host.DebugSetFramePacerWaitableForTest(units.get(), 5'000u);
    const WorkCounterScope seam;
    paced.probe->Repaint(host);
    static_cast<void>(UpdateWindow(paced.window.Hwnd()));
    Require(host.DebugGetFramePacerStateForTest().deferred && host.DebugGetFramePacerStateForTest().waitArmed, "a paint deferred");
    Require(ReleaseSemaphore(units.get(), 1, nullptr) != FALSE, "a frame retires");
    Sleep(50u);
    Require(host.DebugWaitForFrameCreditForTest(100u), "the seam takes the credit");
    const bool presented = PumpUntil(10'000u, [&seam] { return seam.Delta().frame.presents >= 1u; });
    const auto state     = host.DebugGetFramePacerStateForTest();
    std::cout << "    [W3.6] deferred paint after the credit seam: presented " << presented << ", credit " << state.credit << ", armed " << state.waitArmed
              << ", deferred " << state.deferred << ", invariant violations " << seam.Delta().frame.pacerInvariantViolations << '\n';
    host.DebugSetFramePacerWaitableForTest(nullptr);
    Require(presented && ! state.deferred, "a paint deferred before the credit seam is still rendered");
    Require(seam.Delta().frame.pacerInvariantViolations == 0u, "every deferred paint keeps its wake");
}

// Cancelling a wait whose object was signaled a moment before keeps the unit, as the credit or still in the object (the
// drain in DrainWait; review K3: without WaitForThreadpoolWaitCallbacks most rounds lose it). Random spins place the cancel
// before, during and after the callback.
void TestFramePacingCancelRaceKeepsTheUnit()
{
    PacedWindow paced;
    auto& host = paced.Host();
    paced.Settle();
    wil::unique_handle units(CreateSemaphoreW(nullptr, 0, 8, nullptr));
    size_t lost = 0u, spare = 0u, notArmed = 0u;
    constexpr int kRounds = 400;
    for (int round = 0; round < kRounds; ++round)
    {
        host.DebugSetFramePacerWaitableForTest(units.get(), 5'000u); // credit dropped, nothing armed
        paced.probe->Repaint(host);
        static_cast<void>(UpdateWindow(paced.window.Hwnd())); // no unit: the paint defers and arms the wait
        if (! host.DebugGetFramePacerStateForTest().waitArmed)
        {
            ++notArmed;
            continue;
        }
        Require(ReleaseSemaphore(units.get(), 1, nullptr) != FALSE, "a frame retires");
        for (int spin = 0; spin < (round * 7) % 400; ++spin)
            YieldProcessor();
        if (! host.DebugWaitForFrameCreditForTest(0u)) // cancel (drain), then take without waiting
            ++lost;
        while (WaitForSingleObject(units.get(), 0u) == WAIT_OBJECT_0)
            ++spare;
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
            static_cast<void>(DispatchQueuedMessageForTest(message));
    }
    std::cout << "    [W3.6] cancel race, " << kRounds << " rounds: lost " << lost << ", spare " << spare << ", not armed " << notArmed << '\n';
    host.DebugSetFramePacerWaitableForTest(nullptr);
    Require(notArmed == 0u, "every round armed the wait");
    Require(lost == 0u && spare == 0u, "a unit taken by a satisfied wait is never lost when the wait is cancelled");
}

// A foreign message pump (a plain GetMessage/DispatchMessage loop, as the system's size/move, menu and drag loops run) keeps a
// paced host painting: the paced frame is a message posted to the host's own window and the skip timer a WM_TIMER with a timer
// procedure, both delivered by DispatchMessage alone; the host has no idle step a pump must run.
void TestFramePacingForeignMessagePumpKeepsPainting()
{
    PacedWindow paced;
    auto& host = paced.Host();
    paced.Settle();
    const auto foreignPump = [](uint64_t durationMs, auto&& done)
    {
        // A thread timer bounds GetMessage; nothing of DxUi's (no test helper) touches the messages.
        const UINT_PTR timer = SetTimer(nullptr, 0u, 20u, nullptr);
        const uint64_t end   = GetTickCount64() + durationMs;
        MSG message{};
        while (! done() && GetTickCount64() < end && GetMessageW(&message, nullptr, 0u, 0u) > 0)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        KillTimer(nullptr, timer);
        return done();
    };
    // An animation that repaints every tick, then one whose ticks present nothing (the skip timer), then a deferred paint.
    size_t skipping     = 0u;
    paced.probe->onTick = [&paced, &skipping](DxUi::ControlHost& tickHost, uint64_t)
    {
        if (skipping == 0u)
            paced.probe->Repaint(tickHost);
        else
            ++skipping;
        return true;
    };
    const WorkCounterScope animating;
    host.RequestAnimation(*paced.probe);
    const bool presenting = foreignPump(10'000u, [&animating] { return animating.Delta().frame.presents >= 10u; });
    skipping              = 1u;
    const WorkCounterScope skipped;
    const bool ticking = foreignPump(10'000u, [&skipping] { return skipping >= 6u; });
    host.CancelAnimation(*paced.probe);
    wil::unique_handle units(CreateSemaphoreW(nullptr, 0, 8, nullptr));
    host.DebugSetFramePacerWaitableForTest(units.get(), 5'000u);
    const WorkCounterScope deferredScope;
    paced.probe->Repaint(host);
    static_cast<void>(UpdateWindow(paced.window.Hwnd()));
    const bool deferred = host.DebugGetFramePacerStateForTest().deferred;
    Require(ReleaseSemaphore(units.get(), 1, nullptr) != FALSE, "a frame retires");
    const bool rendered = foreignPump(10'000u, [&deferredScope] { return deferredScope.Delta().frame.presents >= 1u; });
    std::cout << "    [W3.6] foreign pump: " << animating.Delta().frame.presents << " presents and " << animating.Delta().wakes.pacedFrame
              << " paced frames animating, " << skipped.Delta().wakes.pacerTimer << " skip timers, deferred paint rendered " << rendered << '\n';
    host.DebugSetFramePacerWaitableForTest(nullptr);
    Require(presenting && animating.Delta().wakes.pacedFrame >= 10u, "paced frames reach the host through a foreign pump");
    Require(ticking && skipped.Delta().wakes.pacerTimer >= 1u, "the skip timer reaches the host through a foreign pump");
    Require(deferred && rendered, "a deferred paint is rendered through a foreign pump");
}

// Text services and UI Automation learn of content after its pixels were presented (review H2): a render that presented
// nothing because no credit was available publishes nothing; the frame that presents it publishes. While the compositor is
// stalled, a wait that times out with the frame still deferred publishes it, so a client waits at most one timeout (250 ms)
// for geometry it would otherwise wait for until the display composes again.
void TestFramePacingUnpresentedRenderPublishesNothing()
{
    PacedWindow paced;
    auto& host = paced.Host();
    auto* root = static_cast<DxUi::Panel*>(host.GetRoot());
    // A rich text editor prepares its layout as content (Control::EnableContentPreparation): what PublishPreparedContent
    // tells text services and UI Automation about.
    auto* field = root->AddChild<DxUi::RichTextEditor>(L"Value");
    field->SetBounds(D2D1::RectF(8.0f, 50.0f, 240.0f, 120.0f));
    field->SetAccessibleName(L"Entry");
    Require(DxUi::DebugSetAccessibilityMutationInterestForTest(paced.window.Hwnd(), true), "the window publishes at each mutation");
    const auto removeInterest =
        wil::scope_exit([&paced] { static_cast<void>(DxUi::DebugSetAccessibilityMutationInterestForTest(paced.window.Hwnd(), std::nullopt)); });
    host.RefreshAccessibilitySnapshot();
    PumpFor(100u);
    paced.Settle();
    const UINT pacedFrame = RegisterWindowMessageW(L"RedSalamanders.DxUi.WindowHost.PacedFrame.v1");
    for (const bool stalled : {false, true})
    {
        wil::unique_handle units(CreateSemaphoreW(nullptr, 0, 8, nullptr));
        host.DebugSetFramePacerWaitableForTest(units.get(), stalled ? 60u : 5'000u);
        field->SetText(stalled ? L"A value laid out again while the compositor is stalled" : L"A longer value that the field lays out again");
        const uint64_t before = Mutations();
        const WorkCounterScope render;
        static_cast<void>(host.RenderInitialFrameForShow()); // a bounded wait that finds no credit: renders, does not present
        const uint64_t afterRender = Mutations();
        std::cout << "    [W3.6] " << (stalled ? "stalled" : "retiring") << ": a render without a credit (" << render.Delta().frame.renders << " renders, "
                  << render.Delta().frame.presents << " presents) published " << afterRender - before << '\n';
        Require(render.Delta().frame.renders == 1u && render.Delta().frame.presents == 0u, "precondition: the render ran and presented nothing");
        Require(afterRender == before, "a render that presented nothing publishes nothing");
        if (! stalled)
        {
            Require(ReleaseSemaphore(units.get(), 1, nullptr) != FALSE, "a frame retires");
            host.DebugRenderDirtyRectForTest(RECT{0, 0, 40, 40}); // takes the credit and presents
            Require(render.Delta().frame.presents == 1u, "precondition: the frame presents");
            Require(Mutations() > afterRender, "the frame that presents publishes the prepared content");
        }
        else
        {
            // Only the paced-frame messages are dispatched, so nothing but the pacer's timeout can publish.
            const uint64_t end = GetTickCount64() + 10'000u;
            while (Mutations() == afterRender && GetTickCount64() < end)
            {
                MSG message{};
                while (PeekMessageW(&message, paced.window.Hwnd(), pacedFrame, pacedFrame, PM_REMOVE))
                    DispatchMessageW(&message);
                Sleep(5u);
            }
            std::cout << "    [W3.6] stalled: " << render.Delta().frame.pacerWaitTimeouts << " timeouts, published " << Mutations() - afterRender
                      << ", presents " << render.Delta().frame.presents << '\n';
            Require(render.Delta().frame.presents == 0u && render.Delta().frame.pacerWaitTimeouts >= 1u && Mutations() > afterRender,
                    "a stalled compositor's first timeout publishes the deferred frame's content");
        }
        host.DebugSetFramePacerWaitableForTest(nullptr);
        PumpFor(100u);
    }
}
