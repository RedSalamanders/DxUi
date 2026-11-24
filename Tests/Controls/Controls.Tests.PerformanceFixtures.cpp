// Performance fixtures on the WindowHost: the entry point of DxUi.ControlTests.exe (plan W0.4; Tests/README.md,
// "Performance fixtures"). Fixtures are defined in Tests/Embedded/Embedded.Tests.PerformanceFixtureFx<N>.h and listed in
// Embedded.Tests.PerformanceFixtureCatalog.h; this file runs the ones whose hosts include the window host. A window frame
// is the host's own WM_PAINT: posted messages other than timers are pumped, then UpdateWindow paints whatever is invalid,
// after the host took a frame-latency credit (W3.6), so the paint renders at once.
// Timers never run here: time reaches the host only through the fixture clock (HostPort::AdvanceTime ->
// DebugAnimationTickForTest), so two runs see the same steps. This executable does not count C++ allocations; a receipt
// says so. Nothing here is edited to add a fixture.
#include "Controls.Tests.PerformanceFixtures.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.PerformanceFixtureFx4.h"
#include "Embedded.Tests.PerformanceFixtureCatalog.h"
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // scenarios are never copied
#include "Embedded.Tests.PerformanceFixtureSelfTest.h"
#pragma warning(pop)

#include "Support.Tests.CallTicks.h"

#include <dwmapi.h>
#include <dxgi.h>
#include <psapi.h>

#include <format>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")

// Instances are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
namespace
{
using namespace PerformanceFixtures;

// A fixture live on a WindowHost of a non-activating test window that forces WARP.
class WindowInstance final : public HostPort
{
public:
    // The accessibility build gate answers as if a UI Automation client listens, whatever the desktop's clients do while the
    // fixture runs (UiaClientsAreListening changes as other processes start and stop listening): the state of a developer
    // desktop and of the retained window receipts. With foreign messages kept from the host (ObserveMessage), the host
    // publishes its snapshot exactly when the fixture's own changes ask for it.
    WindowInstance() : listenersBefore_(DxUi::DebugGetAccessibilitySystemListenersForTest())
    {
        DxUi::DebugSetAccessibilitySystemListenersForTest(true);
    }
    ~WindowInstance() override
    {
        context_.reset();
        window_.reset();
        DxUi::DebugSetAccessibilitySystemListenersForTest(listenersBefore_);
    }
    WindowInstance(const WindowInstance&)            = delete;
    WindowInstance& operator=(const WindowInstance&) = delete;
    WindowInstance(WindowInstance&&)                 = delete;
    WindowInstance& operator=(WindowInstance&&)      = delete;

    void Instantiate(const Definition& definition)
    {
        definition_ = &definition;
        scenario_   = definition.create();
    }
    // forceWarp false only for the hardware attribution's present pass (RunWindowPresentAttributionCommand).
    void Create(const Variant& variant, bool forceWarp = true)
    {
        window_.emplace(DxUi::WindowHost::PresentationMode::HwndSwapChain, forceWarp);
        window_->SetMessageObserver(&WindowInstance::ObserveMessage, this);
        // The host's animation clock is the fixture clock from the start: a delay scheduled before the first tick (a tooltip's
        // show delay) is measured on it, never on GetTickCount64.
        window_->Host().DebugSetAnimationClockForTest(now_);
        auto theme          = DxUi::MakeDefaultThemePalette(true);
        theme.reducedMotion = definition_->reducedMotion;
        window_->Host().SetTheme(theme);
        window_->AllowOuterSizeBeyondDesktop(SIZE{8192, 8192});
        Resize(variant.width, variant.height, variant.dpi);
        context_.emplace(Context{Host::Window, variant, window_->Host(), *this});
        RequireSucceeded(scenario_->Build(*context_), "the performance fixture builds on the window host");
        ShowWindow(window_->Hwnd(), SW_SHOWNOACTIVATE);
    }
    [[nodiscard]] DxUi::WindowHost& Host() noexcept
    {
        return window_->Host();
    }
    [[nodiscard]] HWND Hwnd() const noexcept
    {
        return window_->Hwnd();
    }
    void Step(size_t phase, size_t step)
    {
        scenario_->Step(*context_, phase, step);
    }
    void Frame()
    {
        // WM_TIMER and the system's WM_SYSTIMER (0x118, caret blink) stay queued: they depend on the real clock.
        constexpr UINT kSystemTimer = 0x0118u;
        MSG message{};
        for (size_t count = 0u; count < 4096u; ++count)
        {
            if (! PeekMessageW(&message, nullptr, 0u, WM_TIMER - 1u, PM_REMOVE) && ! PeekMessageW(&message, nullptr, kSystemTimer + 1u, 0xFFFFu, PM_REMOVE))
                break;
            // The test window sits off every monitor, so the system posts WM_MOUSELEAVE as soon as the host's TrackMouseEvent asks for
            // it; dispatching it would turn every pointer move into a leave and a re-entry. Only a step's own Leave reaches the host.
            if (message.message == WM_MOUSELEAVE && message.hwnd == Hwnd())
            {
                Note("dropped queued WM_MOUSELEAVE");
                continue;
            }
            if (events_)
                Note("dequeued " + MessageName(message.message) +
                     (message.hwnd == Hwnd() ? " for the host window"
                      : message.hwnd         ? " for another window"
                                             : " for the thread"));
            if (message.message == WM_PAINT && message.hwnd == Hwnd())
            {
                PaintIfTheHostInvalidated(false);
                continue;
            }
            static_cast<void>(DispatchQueuedMessageForTest(message));
        }
        PaintIfTheHostInvalidated(true);
    }

    // Another process may invalidate any window (RedrawWindow from a desktop tool, which also repaints the frame): Windows
    // then sends the fixture window a WM_PAINT the step never asked for, and the host renders a whole extra frame. The host
    // invalidates only through ControlHost::Invalidate, always the whole client area (a resize included, through OnSize), so
    // the window paints exactly when the host invalidated since its last paint. A region nobody in the fixture asked for is
    // validated unpainted and recorded.
    void PaintIfTheHostInvalidated(bool updateWindow)
    {
        const HWND hwnd = window_->Hwnd();
        RECT invalid{};
        if (GetUpdateRect(hwnd, &invalid, FALSE) == FALSE)
            return;
        const uint64_t invalidations = window_->Host().DebugGetInvalidateCount();
        if (invalidations == paintedInvalidations_)
        {
            Note(std::format(
                "a region the host did not invalidate ({},{})-({},{}) validated unpainted", invalid.left, invalid.top, invalid.right, invalid.bottom));
            static_cast<void>(ValidateRect(hwnd, nullptr));
            return;
        }
        paintedInvalidations_ = invalidations;
        if (updateWindow)
            Note(std::format("UpdateWindow paints an invalid region ({},{})-({},{})", invalid.left, invalid.top, invalid.right, invalid.bottom));
        // W3.6: the paced host renders a WM_PAINT at once only while it holds a frame-latency credit, else it defers to its next
        // paced frame; taking the credit first keeps one render per step, whatever the display's timing.
        Require(window_->Host().DebugWaitForFrameCreditForTest(1'000u), "the window fixture's host takes a frame credit");
        static_cast<void>(UpdateWindow(hwnd));
    }

    // Diagnostics (Tests/README.md, "Performance fixtures"): what reached the host during one step, so a step whose counters
    // differ between two runs names the event that was in one run and not in the other.
    void BeginStepEvents(std::vector<std::string>& events)
    {
        events_ = &events;
        if (const bool listening = UiaClientsAreListening() != FALSE; listening != uiaListening_)
        {
            uiaListening_ = listening;
            Note(listening ? "UI Automation clients started listening (UiaClientsAreListening)" : "UI Automation clients stopped listening");
        }
        snapshotBuilds_ = DxUi::DebugGetAccessibilitySnapshotBuildCountForTest();
    }
    void EndStepEvents()
    {
        if (! events_)
            return;
        if (const uint64_t builds = DxUi::DebugGetAccessibilitySnapshotBuildCountForTest() - snapshotBuilds_; builds != 0u)
            Note(std::format("{} accessibility snapshot build(s), any thread", builds));
        const auto wake = window_->Host().DebugGetFrameScheduleWakeForTest();
        if (wake.kind != 0u)
            Note(std::format(
                "frame schedule waits: kind {} due {}", wake.kind, wake.dueTickMs >= now_ ? std::format("now+{}", wake.dueTickMs - now_) : "earlier"));
        events_ = nullptr;
    }

    bool Pointer(const DxUi::PointerEvent& event) override
    {
        const auto x          = static_cast<LONG>(std::lround(event.xPixels));
        const auto y          = static_cast<LONG>(std::lround(event.yPixels));
        const LPARAM position = MAKELPARAM(static_cast<short>(x), static_cast<short>(y));
        const WPARAM keys     = static_cast<WPARAM>(event.modifiers);
        switch (event.action)
        {
            case DxUi::PointerAction::Move: SendMessageW(Hwnd(), WM_MOUSEMOVE, keys, position); break;
            case DxUi::PointerAction::Down: SendMessageW(Hwnd(), WM_LBUTTONDOWN, keys | MK_LBUTTON, position); break;
            case DxUi::PointerAction::Up: SendMessageW(Hwnd(), WM_LBUTTONUP, keys, position); break;
            case DxUi::PointerAction::MiddleDown: SendMessageW(Hwnd(), WM_MBUTTONDOWN, keys | MK_MBUTTON, position); break;
            case DxUi::PointerAction::MiddleUp: SendMessageW(Hwnd(), WM_MBUTTONUP, keys, position); break;
            case DxUi::PointerAction::Leave: SendMessageW(Hwnd(), WM_MOUSELEAVE, 0u, 0); break; // sent, so Frame never drops it
            case DxUi::PointerAction::Cancel: SendMessageW(Hwnd(), WM_CANCELMODE, 0u, 0); break;
            case DxUi::PointerAction::Wheel:
            {
                POINT screen{x, y};
                static_cast<void>(ClientToScreen(Hwnd(), &screen));
                SendMessageW(Hwnd(),
                             WM_MOUSEWHEEL,
                             MAKEWPARAM(static_cast<WORD>(event.modifiers), static_cast<short>(std::lround(event.wheelDelta))),
                             MAKELPARAM(static_cast<short>(screen.x), static_cast<short>(screen.y)));
                break;
            }
        }
        return true;
    }
    bool Key(UINT virtualKey, bool down, UINT) override
    {
        const LPARAM flags = down ? 1 : static_cast<LPARAM>(0xC0000001u);
        SendMessageW(Hwnd(), down ? WM_KEYDOWN : WM_KEYUP, virtualKey, flags);
        return true;
    }
    bool Character(wchar_t character, UINT) override
    {
        SendMessageW(Hwnd(), WM_CHAR, character, 1);
        return true;
    }
    void AdvanceTime(uint64_t milliseconds) override
    {
        now_ += milliseconds;
        window_->Host().DebugSetAnimationClockForTest(now_);
        const bool pending = window_->Host().DebugAnimationTickForTest(now_);
        Note(std::format("fixture tick (+{} ms){}", milliseconds, pending ? ", animation still pending" : ""));
    }
    // The client area becomes `width` x `height` device pixels at `dpi`, the DPI delivered as Windows would deliver it.
    void Resize(UINT width, UINT height, float dpi) override
    {
        const HWND hwnd = window_->Hwnd();
        RECT outer{};
        GetWindowRect(hwnd, &outer);
        if (window_->Host().GetDpi() != dpi)
        {
            const auto value = static_cast<WORD>(std::lround(dpi));
            bool handled     = false;
            static_cast<void>(window_->Host().HandleMessage(hwnd, WM_DPICHANGED, MAKEWPARAM(value, value), reinterpret_cast<LPARAM>(&outer), handled));
            Require(window_->Host().GetDpi() == dpi, "the window host takes the fixture's DPI");
        }
        RECT client{};
        GetWindowRect(hwnd, &outer);
        GetClientRect(hwnd, &client);
        SetWindowPos(hwnd,
                     nullptr,
                     0,
                     0,
                     (outer.right - outer.left) - (client.right - client.left) + static_cast<LONG>(width),
                     (outer.bottom - outer.top) - (client.bottom - client.top) + static_cast<LONG>(height),
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        GetClientRect(hwnd, &client);
        Require(client.right == static_cast<LONG>(width) && client.bottom == static_cast<LONG>(height), "the window host takes the fixture's size");
    }
    [[nodiscard]] uint64_t NowMs() const noexcept override
    {
        return now_;
    }

private:
    [[nodiscard]] static std::string MessageName(UINT message)
    {
        switch (message)
        {
            case WM_PAINT: return "WM_PAINT";
            case WM_TIMER: return "WM_TIMER";
            case WM_GETOBJECT: return "WM_GETOBJECT";
            case WM_SETTINGCHANGE: return "WM_SETTINGCHANGE";
            case WM_THEMECHANGED: return "WM_THEMECHANGED";
            case WM_DISPLAYCHANGE: return "WM_DISPLAYCHANGE";
            case WM_DWMCOLORIZATIONCOLORCHANGED: return "WM_DWMCOLORIZATIONCOLORCHANGED";
            case WM_MOUSELEAVE: return "WM_MOUSELEAVE";
            case WM_MOUSEMOVE: return "WM_MOUSEMOVE";
            case WM_SETCURSOR: return "WM_SETCURSOR";
            case WM_NCHITTEST: return "WM_NCHITTEST";
            case WM_WINDOWPOSCHANGING: return "WM_WINDOWPOSCHANGING";
            case WM_WINDOWPOSCHANGED: return "WM_WINDOWPOSCHANGED";
            case WM_SIZE: return "WM_SIZE";
            case WM_ERASEBKGND: return "WM_ERASEBKGND";
            case WM_NCPAINT: return "WM_NCPAINT";
            case WM_ACTIVATEAPP: return "WM_ACTIVATEAPP";
            case WM_IME_NOTIFY: return "WM_IME_NOTIFY";
            default: break;
        }
        if (message >= 0xC000u)
        {
            // A registered message: its name shares the atom table of clipboard formats.
            wchar_t name[128]{};
            const int length = GetClipboardFormatNameW(message, name, static_cast<int>(std::size(name)));
            std::string text = "registered ";
            for (int index = 0; index < length; ++index)
                text.push_back(name[index] < 0x80 ? static_cast<char>(name[index]) : '?');
            return text;
        }
        return std::format("message 0x{:04X}", message);
    }
    void Note(std::string event)
    {
        if (events_)
            events_->push_back(std::move(event));
    }
    // A message another thread sends is the desktop's, never the fixture's: a UI Automation client's WM_GETOBJECT (which
    // would publish the host's accessibility snapshot in the middle of a step and split the step's one coalesced
    // publication in two), its WM_GETTEXT, a settings or theme broadcast. DefWindowProc answers it and the host never sees
    // it, so a step's counters cannot depend on what else runs on the desktop. The fixture's own input is sent from this
    // thread or posted by the host itself.
    static bool ObserveMessage(void* context, UINT message, WPARAM, LPARAM lParam)
    {
        auto& self         = *static_cast<WindowInstance*>(context);
        const DWORD send   = InSendMessageEx(nullptr);
        const bool foreign = (send & (ISMEX_SEND | ISMEX_NOTIFY | ISMEX_CALLBACK)) != 0u;
        if (self.events_)
        {
            // Every message is recorded, the step's own input included: both runs send that input, so only a difference shows.
            std::string origin = foreign ? " from another thread, answered without the host" : "";
            if (message == WM_GETOBJECT)
                origin += std::format(" (object id {})", static_cast<LONG>(static_cast<DWORD>(lParam)));
            self.Note("window procedure: " + MessageName(message) + origin);
        }
        return foreign;
    }

    const Definition* definition_ = nullptr;
    // Declared before the window it may own models for, so the window and its host are destroyed first.
    std::unique_ptr<Scenario> scenario_;
    std::optional<AttachedHostWindow> window_;
    std::optional<Context> context_;
    uint64_t now_ = kClockOriginMs;
    std::optional<bool> listenersBefore_;
    uint64_t paintedInvalidations_    = 0u;
    std::vector<std::string>* events_ = nullptr;
    std::optional<bool> uiaListening_;
    uint64_t snapshotBuilds_ = 0u;
};

[[nodiscard]] PROCESS_MEMORY_COUNTERS_EX ProcessMemory()
{
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    Require(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE, "process memory");
    return memory;
}

// What reached the host in each step of a run (WindowInstance::BeginStepEvents), indexed like StepAt: phase by phase.
// Real-time durations are kept apart: they always differ and are printed only beside a difference.
struct WindowRunEvents
{
    std::vector<std::vector<std::string>> steps;
    std::vector<int64_t> durationsUs;
};

// One run on the window host, as RunEmbedded does on the embedded host (counters per step after a tracker sweep, memory
// per phase), without the C++ allocation sink, retained statistics or video memory, which this host does not report.
RunRecord RunWindow(const Definition& definition, const Variant& variant, WindowRunEvents* events = nullptr)
{
    Require(definition.Built() && Supports(definition.hosts, Host::Window), "the fixture is built for the window host");
    Require(! variant.hardwareOnly, "the window runner renders on WARP only");
    Require(! definition.settlePreparation, "a fixture that settles preparation within a step runs on the embedded host only");
    RunRecord run;
    run.definition            = &definition;
    run.variant               = &variant;
    run.host                  = Host::Window;
    run.cppAllocationsCounted = false;
    run.adapter               = AdapterRecord{"Microsoft Basic Render Driver (WindowHost forceWarp)", 0x1414u, 0u, true, 0u, 0u};
    run.entryPrivateBytes     = ProcessMemory().PrivateUsage;
    run.phases.resize(definition.phases.size());
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
    {
        run.phases[phase].index = phase;
        run.phases[phase].steps.reserve(definition.phases[phase].steps);
    }
    run.memory.reserve(definition.phases.size() + 3u);
    const auto sample = [&](std::string_view at, std::string_view phase)
    {
        const auto process = ProcessMemory();
        MemorySample value{};
        value.at              = at;
        value.phase           = phase;
        value.privateBytes    = process.PrivateUsage;
        value.workingSetBytes = process.WorkingSetSize;
        run.memory.push_back(value);
    };
    std::vector<DxUi::Detail::TrackedResource> storage(262144u);
    sample("device", {});
    WindowInstance instance;
    instance.Instantiate(definition);
    instance.Create(variant);
    sample("built", {});
    {
        DxUi::Detail::ResourceLifetimeTracker tracker(storage);
        for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        {
            for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
            {
                const auto started = std::chrono::steady_clock::now();
                if (events)
                    instance.BeginStepEvents(events->steps.emplace_back());
                const DxUi::Detail::WorkCounterScope scope;
                instance.Step(phase, step);
                instance.Frame();
                static_cast<void>(tracker.Sweep());
                run.phases[phase].steps.push_back(StepRecord{scope.Delta(), 0u});
                instance.EndStepEvents();
                if (events)
                    events->durationsUs.push_back(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count());
            }
            sample("phase", definition.phases[phase].name);
        }
        run.trackedResources = tracker.Held();
        run.trackerOverflow  = tracker.Overflow();
    }
    ShowWindow(instance.Hwnd(), SW_HIDE);
    sample("hidden", {});
    return run;
}

// The flat index of a phase's step, as StepAt counts.
[[nodiscard]] size_t FlatStepIndex(const Definition& definition, std::string_view phaseName, size_t step)
{
    size_t index = 0u;
    for (const Phase& phase : definition.phases)
    {
        if (phase.name == phaseName)
            return index + step;
        index += phase.steps;
    }
    return index;
}

// Prints one step of both runs: every counter that differs, then what reached the host in either run and not in the other
// (a multiset difference), then both event lists in order. Printed only when the gate fails.
void PrintStepDifference(const RunRecord& first, const RunRecord& second, const WindowRunEvents& a, const WindowRunEvents& b, size_t index)
{
    const std::optional<PhaseStep> at = StepAt(*first.definition, index);
    if (! at || index >= a.steps.size() || index >= b.steps.size())
        return;
    std::cerr << "  -- " << first.definition->phases[at->phase].name << " step " << at->step << " (run times " << a.durationsUs[index] << " us and "
              << b.durationsUs[index] << " us)\n";
    std::vector<std::pair<std::string, uint64_t>> values;
    ForEachCounter(first.phases[at->phase].steps[at->step], [&](std::string_view name, uint64_t value) { values.emplace_back(std::string(name), value); });
    size_t counter = 0u;
    ForEachCounter(second.phases[at->phase].steps[at->step],
                   [&](std::string_view, uint64_t value)
    {
        if (values[counter].second != value)
            std::cerr << "     counter " << values[counter].first << ": " << values[counter].second << " then " << value << '\n';
        ++counter;
    });
    std::map<std::string, int> balance;
    for (const std::string& event : a.steps[index])
        ++balance[event];
    for (const std::string& event : b.steps[index])
        --balance[event];
    for (const auto& [event, count] : balance)
        if (count != 0)
            std::cerr << "     only in the " << (count > 0 ? "first" : "second") << " run (x" << std::abs(count) << "): " << event << '\n';
    for (const auto* run : {&a, &b})
    {
        std::cerr << "     " << (run == &a ? "first" : "second") << " run events:";
        for (const std::string& event : run->steps[index])
            std::cerr << "\n       " << event;
        std::cerr << '\n';
    }
}

void RequireDeterministic(const RunRecord& first,
                          const RunRecord& second,
                          const WindowRunEvents* firstEvents  = nullptr,
                          const WindowRunEvents* secondEvents = nullptr)
{
    if (const std::optional<Difference> difference = FirstDifference(first, second))
    {
        std::cerr << "Nondeterministic performance fixture: " << Describe(first, *difference) << '\n';
        if (firstEvents && secondEvents)
        {
            // The differing step and the one before it: work that slips by a step shows in both.
            const size_t index = FlatStepIndex(*first.definition, difference->phase, difference->step);
            if (index > 0u)
                PrintStepDifference(first, second, *firstEvents, *secondEvents, index - 1u);
            PrintStepDifference(first, second, *firstEvents, *secondEvents, index);
        }
        Require(false, "two consecutive WARP runs of a performance fixture record equal counters");
    }
}

[[nodiscard]] uint64_t HashCapture(const DxUi::WindowHostBitmapCapture& capture) noexcept
{
    uint64_t hash = 0xCBF29CE484222325ull;
    for (const uint8_t byte : capture.bgraPixels)
    {
        hash ^= byte;
        hash *= 0x100000001B3ull;
    }
    return hash;
}

// Plays a fixture on the window host, one frame per step, and hashes a full frame of the host after each step
// (DebugCaptureBitmap renders the whole client area and reads it back). before(instance, phase, step) runs before a step.
template <typename Before> std::vector<uint64_t> WindowFrameHashes(const Definition& definition, const Variant& variant, Before&& before)
{
    std::vector<uint64_t> hashes;
    WindowInstance instance;
    instance.Instantiate(definition);
    instance.Create(variant);
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
        {
            before(instance, phase, step);
            instance.Step(phase, step);
            instance.Frame();
            DxUi::WindowHostBitmapCapture capture;
            Require(instance.Host().DebugCaptureBitmap(capture, true), "a window fixture frame is captured");
            hashes.push_back(HashCapture(capture));
        }
    return hashes;
}

[[nodiscard]] uint64_t StepCounter(const StepRecord& step, std::string_view name)
{
    const std::optional<uint64_t> value = CounterValue(step, name);
    Require(value.has_value(), "a W2.8 gate names a counter that exists");
    return value.value_or(0u);
}
} // namespace
#pragma warning(pop)

void TestPerformanceFixtureFrameworkOnWindowHost()
{
    const Definition& definition = SelfTest::kDefinition;
    WindowRunEvents firstEvents;
    WindowRunEvents secondEvents;
    const RunRecord first  = RunWindow(definition, SelfTest::kVariants[0], &firstEvents);
    const RunRecord second = RunWindow(definition, SelfTest::kVariants[0], &secondEvents);
    RequireDeterministic(first, second, &firstEvents, &secondEvents);
    Require(first.trackerOverflow == 0u, "the tracker observed every resource the fixture created");
    for (const StepRecord& step : first.Phase("clean").steps)
        Require(step.work.frame.renders == 0u && step.work.frame.paintExecutions == 0u && DxUi::Detail::TotalCreated(step.work) == 0u,
                "a clean self-test step renders nothing and creates nothing on the window host");
    for (const StepRecord& step : first.Phase("text").steps)
        Require(step.work.frame.renders == 1u && step.work.frame.paintExecutions == SelfTest::kVisibleControls,
                "a text change renders once and paints every visible control on the window host");
    Require(Summarize(first.Phase("hover"), "frame.treeVisitsHitTest").minimum > 0u, "every pointer step reaches the window host's hit test");
    std::ostringstream receipt;
    WriteReceiptJson(receipt, first);
    Require(receipt.str().find("\"host\":\"window\"") != std::string::npos && receipt.str().find("\"cppAllocations\":\"not counted") != std::string::npos,
            "a window receipt names its host and says it does not count C++ allocations");
}

void TestPerformanceFixturesAreDeterministicOnWindowHost()
{
    for (const Definition* definition : kCatalog)
    {
        if (definition->interactiveOnly)
        {
            // Its own gate is in the Menu suite and runs only under test.ps1 -Interactive.
            std::cerr << "    [FIXTURE] " << InteractiveNotRun(*definition) << '\n';
            continue;
        }
        if (! definition->Built() || ! Supports(definition->hosts, Host::Window))
            continue;
        for (const Variant& variant : definition->variants)
        {
            if (variant.hardwareOnly || variant.receiptOnly)
                continue;
            const auto started = std::chrono::steady_clock::now();
            WindowRunEvents firstEvents;
            WindowRunEvents secondEvents;
            const RunRecord first  = RunWindow(*definition, variant, &firstEvents);
            const RunRecord second = RunWindow(*definition, variant, &secondEvents);
            RequireDeterministic(first, second, &firstEvents, &secondEvents);
            std::cerr << "    [FIXTURE] " << definition->id << '@' << variant.name << " (window, WARP): two runs record equal counters ("
                      << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count() << " ms)\n";
        }
    }
}

// W3.1's frame-schedule gate on the window host: every window fixture ticks only due requesters and never takes the legacy
// whole-tree path; FX1 editable (G1) ticks its caret at most 20 times in 10 s of the fixture clock. Real dispatcher wakes are
// counted by TestFrameScheduleNativeFx1CaretWakesInRealTime, since this runner never dispatches a timer.
void TestFrameScheduleGatesOnWindowFixtures()
{
    for (const Definition* definition : kCatalog)
    {
        // An interactive-only fixture takes the foreground: its own gate runs in the interactive Menu suite.
        if (definition->interactiveOnly || ! definition->Built() || ! Supports(definition->hosts, Host::Window))
            continue;
        const Variant& variant = definition->variants.front();
        if (variant.hardwareOnly || variant.receiptOnly)
            continue;
        const RunRecord run    = RunWindow(*definition, variant);
        uint64_t editableTicks = 0u;
        uint64_t readOnlyTicks = 0u;
        for (size_t phase = 0u; phase < run.phases.size(); ++phase)
            for (const StepRecord& step : run.phases[phase].steps)
            {
                Require(step.work.frame.tickCalls <= step.work.frame.tickRequesters && step.work.frame.tickLegacyTreeWalks == 0u &&
                            step.work.frame.treeVisitsTick == 0u,
                        "a window fixture step ticks only due requesters and never walks the tree");
                if (definition == &Fx1::kDefinition && phase == Fx1::kEditable)
                    editableTicks += step.work.frame.tickCalls;
                if (definition == &Fx1::kDefinition && phase == Fx1::kReadOnly)
                    readOnlyTicks += step.work.frame.tickCalls;
            }
        if (definition == &Fx1::kDefinition)
        {
            std::cerr << "    [FIXTURE] FX1 editable (window, fixture clock): " << editableTicks << " caret ticks in 10 s\n";
            // W3.2: the caret blinks for the 5 s caret timeout FX1 pins (10 flips at 530 ms), then nothing is armed; the read-only
            // caret is steady.
            Require(editableTicks <= 20u && editableTicks >= 9u, "G1: FX1 editable ticks its caret at most 20 times in 10 s on the window host");
            Require(readOnlyTicks == 0u, "W3.2: FX1 read-only ticks nothing on the window host");
        }
    }
}

PerformanceFixtures::RunRecord RunWindowFixture(const PerformanceFixtures::Definition& definition, const PerformanceFixtures::Variant& variant)
{
    return RunWindow(definition, variant);
}

// W2.8's gate on the window host: FX12 (default and typography-120) forces no full visual re-preparation and creates,
// evicts or trims no text format in a measured step; FX11's Tree expand and collapse phases (the transitions that animate a
// color, a row's fade) create no brush after their first frame and draw through the host's transition brushes.
void TestFormatBrushCacheGatesOnWindowFixtures()
{
    for (const char* name : {"default", "typography-120"})
    {
        const Variant* variant = Fx12::kDefinition.FindVariant(name);
        Require(variant != nullptr, "FX12 declares the W2.8 variants");
        const RunRecord run = RunWindow(Fx12::kDefinition, *variant);
        for (size_t phase = 0u; phase < Fx12::kPhases.size(); ++phase)
        {
            if (Fx12::kPhases[phase].kind != PhaseKind::Measured)
                continue;
            const auto& steps = run.phases[phase].steps;
            for (size_t step = 0u; step < steps.size(); ++step)
            {
                const uint64_t full    = StepCounter(steps[step], "frame.fullVisualPreparations");
                const uint64_t trims   = StepCounter(steps[step], "text.formatCacheTrims");
                const uint64_t evicted = StepCounter(steps[step], "text.formatEvictions");
                const uint64_t created = StepCounter(steps[step], "resources.textFormat.created");
                if (full != 0u || trims != 0u || evicted != 0u || created != 0u)
                    std::cerr << "FX12@" << name << " (window) " << Fx12::kPhases[phase].name << " step " << step << ": full preparations " << full
                              << ", trims " << trims << ", formats evicted " << evicted << ", formats created " << created << '\n';
                Require(full == 0u, "W2.8: an FX12 frame forces no full visual re-preparation on the window host");
                Require(trims == 0u && evicted == 0u && created == 0u, "W2.8: an FX12 frame creates, evicts and trims no text format on the window host");
            }
        }
    }
    const RunRecord motion = RunWindow(Fx11::kDefinition, Fx11::kVariants[0]);
    for (size_t rate = 0u; rate < Fx11::kRates.size(); ++rate)
        for (const Fx11::Transition transition : {Fx11::Transition::TreeExpand, Fx11::Transition::TreeCollapse})
        {
            const size_t phase = Fx11::PhaseIndex(rate, transition);
            const auto& steps  = motion.phases[phase].steps;
            uint64_t reuses    = 0u;
            for (size_t step = 0u; step < steps.size(); ++step)
            {
                reuses += StepCounter(steps[step], "frame.brushColorReuses");
                const uint64_t created = StepCounter(steps[step], "resources.brush.created");
                if (step > 0u && created != 0u)
                    std::cerr << "FX11 (window) " << Fx11::kPhases[phase].name << " step " << step << ": " << created << " brushes created\n";
                Require(step == 0u || created == 0u, "W2.8: an FX11 color transition creates no brush after its first frame on the window host");
            }
            Require(reuses > 0u, "W2.8: an FX11 Tree fade draws its colors through the window host's transition brushes");
        }
}

// W2.8 on the window host: a device lost in the middle of a Tree fade drops both brush maps, and every frame equals that of an
// uninterrupted run; a DPI change with FX12's 120 typographies live creates and evicts no text format, and the frame equals a
// fresh host's (every registered retained cache dropped).
void TestFormatBrushCachesAcrossDeviceLossAndDpiOnWindowHost()
{
    const size_t lossPhase                = Fx11::PhaseIndex(1u, Fx11::Transition::TreeExpand); // 60 Hz
    constexpr size_t lossStep             = 6u;
    const std::vector<uint64_t> steady    = WindowFrameHashes(Fx11::kDefinition, Fx11::kVariants[0], [](WindowInstance&, size_t, size_t) {});
    bool lost                             = false;
    const std::vector<uint64_t> recovered = WindowFrameHashes(Fx11::kDefinition,
                                                              Fx11::kVariants[0],
                                                              [&](WindowInstance& instance, size_t phase, size_t step)
    {
        if (phase != lossPhase || step != lossStep)
            return;
        instance.Host().DebugSimulateDeviceLoss();
        Require(instance.Host().DebugGetBrushCacheSize() == 0u, "W2.8: a device loss drops the keyed brushes");
        lost = true;
    });
    Require(lost && steady.size() == recovered.size(), "both FX11 plays render every step");
    for (size_t index = 0u; index < steady.size(); ++index)
        if (steady[index] != recovered[index])
        {
            const std::optional<PhaseStep> at = StepAt(Fx11::kDefinition, index);
            std::cerr << "FX11 (window) " << Fx11::kPhases[at->phase].name << " step " << at->step << " differs after the device loss\n";
            Require(false, "W2.8: frames after a device loss in the middle of a Tree fade equal an uninterrupted run's");
        }

    const Variant& typography = *Fx12::kDefinition.FindVariant("typography-120");
    WindowInstance instance;
    instance.Instantiate(Fx12::kDefinition);
    instance.Create(typography);
    for (size_t step = 0u; step < Fx12::kPhases[Fx12::kWarm].steps; ++step)
    {
        instance.Step(Fx12::kWarm, step);
        instance.Frame();
    }
    const size_t formats = instance.Host().DebugGetConfiguredTextFormatCount();
    Require(formats >= 120u, "the typography scene holds its 120 live formats");
    {
        const DxUi::Detail::WorkCounterScope scope;
        instance.Resize(typography.width, typography.height, 144.0f);
        instance.Host().Invalidate();
        instance.Frame();
        const DxUi::Detail::WorkCounters work = scope.Delta();
        Require(work.resources.textFormat.created == 0u && work.text.formatEvictions == 0u && work.text.formatCacheTrims == 0u,
                "W2.8: a DPI change keeps every live format on the window host");
        std::cerr << "    [W2.8] FX12@typography-120 at 144 DPI (window): " << work.frame.fullVisualPreparations << " full visual preparations\n";
    }
    Require(instance.Host().DebugGetConfiguredTextFormatCount() == formats, "the format map is unchanged by the DPI change");
    DxUi::WindowHostBitmapCapture incremental;
    DxUi::WindowHostBitmapCapture fresh;
    Require(instance.Host().DebugCaptureBitmap(incremental, true), "capture after the DPI change");
    instance.Host().DebugDropRetainedStateForTest();
    Require(instance.Host().DebugCaptureBitmap(fresh, true), "capture after dropping every retained cache");
    Require(incremental.bgraPixels == fresh.bgraPixels, "W2.8: after a DPI change with 120 formats live the window host draws what a fresh host draws");
}

int RunWindowFixtureFrameHashCommand(int argc, wchar_t** argv)
{
    if (argc != 4)
    {
        std::fputs("--fixture-frame-hashes requires FX<N>[@variant] and an output path.\n", stderr);
        return 2;
    }
    const FixtureSelection selection = ParseSelection(argv[2]);
    const Definition* definition     = FindFixture(selection.id);
    const Variant* variant           = definition ? FindVariant(*definition, selection.variant) : nullptr;
    if (! definition || definition->interactiveOnly || ! definition->Built() || ! Supports(definition->hosts, Host::Window) || ! variant ||
        variant->hardwareOnly)
    {
        std::fputs("--fixture-frame-hashes names no WARP variant of a fixture built for the window host.\n", stderr);
        return 2;
    }
    const std::vector<uint64_t> hashes = WindowFrameHashes(*definition, *variant, [](WindowInstance&, size_t, size_t) {});
    const std::filesystem::path path(argv[3]);
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    Require(bool(output), "frame hash file");
    size_t index = 0u;
    for (size_t phase = 0u; phase < definition->phases.size(); ++phase)
        for (size_t step = 0u; step < definition->phases[phase].steps; ++step)
            output << definition->phases[phase].name << ' ' << step << ' ' << std::hex << hashes[index++] << std::dec << '\n';
    output.close();
    Require(bool(output), "frame hashes written");
    std::cout << definition->id << '@' << variant->name << " (window): " << hashes.size() << " frame hashes written to " << path.string() << '\n';
    return 0;
}

// W3.3's gate on FX14 on the window host, through the fixture clock: the tooltip is ticked once at its show deadline and
// once at its hide deadline, never between them. Real dispatcher wakes are counted by
// TestTooltipDeadlinesWakeOnlyAtShowAndHideInRealTime (Tooltip suite), since this runner never dispatches a timer.
void TestTooltipDeadlineGatesOnWindowFx14()
{
    const RunRecord run   = RunWindow(Fx14::kDefinition, Fx14::kVariants[0]);
    uint64_t tooltipTicks = 0u;
    for (const Phase& phase : Fx14::kPhases)
    {
        if (phase.kind != PhaseKind::Measured)
            continue;
        const uint64_t ticks    = Summarize(run.Phase(phase.name), "frame.tooltipDeadlineTicks").total;
        const uint64_t controls = Summarize(run.Phase(phase.name), "frame.tickCalls").total;
        std::cerr << "    [FIXTURE] FX14 " << phase.name << " (window, fixture clock): " << ticks << " tooltip deadline ticks, " << controls
                  << " control Tick calls\n";
        const bool atDeadline = phase.name == "show" || phase.name == "hide";
        Require(ticks == (atDeadline ? 1u : 0u), "FX14: the tooltip is ticked once at its show and once at its hide deadline, never between them");
        Require(controls == 0u, "FX14: no Tick call between the deadlines");
        tooltipTicks += ticks;
    }
    Require(tooltipTicks == 2u, "G1: FX14 ticks the tooltip twice per show on the window host (fixture clock), at its show and its hide deadline");
}

int RunWindowFixtureReceiptCommand(int argc, wchar_t** argv)
{
    const bool interactive = argc == 5 && std::wstring_view(argv[4]) == L"--interactive";
    if (argc != 4 && ! interactive)
    {
        std::fputs("--fixture-receipt requires FX<N>[@variant] and an output path, and --interactive for an interactive-only fixture.\n", stderr);
        return 2;
    }
    const FixtureSelection selection = ParseSelection(argv[2]);
    const Definition* definition     = FindFixture(selection.id);
    const Variant* variant           = definition ? FindVariant(*definition, selection.variant) : nullptr;
    if (definition && definition->interactiveOnly)
    {
        // Only performance-fixtures.ps1 -Interactive passes --interactive, from a child of the desktop lease.
        if (! interactive)
        {
            std::cout << InteractiveNotRun(*definition) << '\n';
            return 0;
        }
        if (! variant)
        {
            std::fputs("--fixture-receipt names no variant of that fixture.\n", stderr);
            return 2;
        }
        return RunInteractiveFixtureReceipt(*definition, *variant, std::filesystem::path(argv[3]));
    }
    if (interactive)
    {
        std::fputs("--interactive applies only to an interactive-only fixture.\n", stderr);
        return 2;
    }
    if (! definition || ! definition->Built() || ! Supports(definition->hosts, Host::Window) || ! variant || variant->hardwareOnly)
    {
        std::fputs("--fixture-receipt names no WARP variant of a fixture built for the window host.\n", stderr);
        return 2;
    }
    const RunRecord run = RunWindow(*definition, *variant);
    const std::filesystem::path path(argv[3]);
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    Require(bool(output), "performance fixture receipt file");
    WriteReceiptJson(output, run);
    output.close();
    Require(bool(output), "performance fixture receipt written");
    return 0;
}

namespace
{
// A WindowHost frame's public metrics (DxUi::SetFrameMetricSink), in microseconds of wall time.
struct PresentFrameMetrics
{
    uint64_t totalUs   = 0u;
    uint64_t renderUs  = 0u;
    uint64_t presentUs = 0u;
    uint32_t frames    = 0u;
};
void CollectPresentFrameMetric(void* context, std::wstring_view metric, uint64_t value) noexcept
{
    auto& metrics = *static_cast<PresentFrameMetrics*>(context);
    if (metric == L"dxui.frame.total_us")
    {
        metrics.totalUs = value;
        ++metrics.frames;
    }
    else if (metric == L"dxui.frame.render_us")
        metrics.renderUs = value;
    else if (metric == L"dxui.frame.present_us")
        metrics.presentUs = value;
}

[[nodiscard]] uint64_t PresentThreadCycles()
{
    ULONG64 cycles = 0u;
    Require(QueryThreadCycleTime(GetCurrentThread(), &cycles) != FALSE, "present attribution thread cycles");
    return cycles;
}

void WritePresentJsonString(std::ostream& output, std::wstring_view text)
{
    output << '"';
    for (const wchar_t c : text)
        output << (c == L'"' || c == L'\\' ? "\\" : "") << (c >= 0x20 && c < 0x7F ? static_cast<char>(c) : '?');
    output << '"';
}
} // namespace

// DxUi.ControlTests.exe --present-attribution <output.json> <FX<N>[@variant]> Hardware|Warp <minFrames> <minMilliseconds>: the
// window-host pass of the hardware attribution method (plan W0.7; Specs/Core/Core_PerformanceAndResources.md, "Hardware
// attribution method"). Full frames (ControlHost::Invalidate with nothing changed) of a fixture on a WindowHost of the
// non-activating test window, which sits off every monitor as the window runner places it: nothing takes focus, moves the
// pointer or covers the desktop. Each frame is the host's own WM_PAINT (UpdateWindow), which renders the whole client area and
// calls Present(1, 0). The host's public frame metrics give its render and Present wall times, QueryThreadCycleTime around the
// paint the UI-thread cycles. DwmFlush after each frame lets the compositor take the frame first, so Present measures its own
// cost rather than a wait for a free buffer.
int RunWindowPresentAttributionCommand(int argc, wchar_t** argv)
{
    size_t minimumFrames = 0u;
    size_t milliseconds  = 0u;
    const auto parse     = [](std::wstring_view text, size_t& value)
    {
        value = 0u;
        if (text.empty() || text.size() > 9u)
            return false;
        for (const wchar_t c : text)
        {
            if (c < L'0' || c > L'9')
                return false;
            value = value * 10u + static_cast<size_t>(c - L'0');
        }
        return true;
    };
    if (argc != 7 || (std::wstring_view(argv[4]) != L"Hardware" && std::wstring_view(argv[4]) != L"Warp") || ! parse(argv[5], minimumFrames) ||
        ! parse(argv[6], milliseconds) || minimumFrames == 0u)
    {
        std::fputs("--present-attribution requires an output path, FX<N>[@variant], Hardware|Warp, minimum frames and minimum milliseconds.\n", stderr);
        return 2;
    }
    const FixtureSelection selection = ParseSelection(argv[3]);
    const Definition* definition     = FindFixture(selection.id);
    const Variant* variant           = definition ? FindVariant(*definition, selection.variant) : nullptr;
    if (! definition || ! definition->Built() || definition->interactiveOnly || ! Supports(definition->hosts, Host::Window) || ! variant ||
        definition->settlePreparation)
    {
        std::fputs("--present-attribution names no variant of a fixture built for the window host.\n", stderr);
        return 2;
    }
    const bool hardware = std::wstring_view(argv[4]) == L"Hardware";

    // The adapter a hardware WindowHost device lands on: the default adapter, as the host creates its device with none named.
    std::wstring adapterName = L"Microsoft Basic Render Driver (WindowHost forceWarp)";
    UINT vendorId = 0x1414u, deviceId = 0u;
    uint64_t driver = 0u;
    std::vector<HMONITOR> adapterMonitors;
    if (hardware)
    {
        wil::com_ptr<IDXGIFactory1> factory;
        Require(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(factory.put()))), "present attribution DXGI factory");
        wil::com_ptr<IDXGIAdapter1> adapter;
        Require(SUCCEEDED(factory->EnumAdapters1(0u, adapter.put())), "present attribution default adapter");
        DXGI_ADAPTER_DESC1 desc{};
        Require(SUCCEEDED(adapter->GetDesc1(&desc)), "present attribution adapter description");
        adapterName = desc.Description;
        vendorId    = desc.VendorId;
        deviceId    = desc.DeviceId;
        LARGE_INTEGER version{};
        if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version)))
            driver = static_cast<uint64_t>(version.QuadPart);
        wil::com_ptr<IDXGIOutput> output;
        for (UINT index = 0u; SUCCEEDED(adapter->EnumOutputs(index, output.put())); ++index)
        {
            DXGI_OUTPUT_DESC outputDesc{};
            if (SUCCEEDED(output->GetDesc(&outputDesc)))
                adapterMonitors.push_back(outputDesc.Monitor);
            output.reset();
        }
    }

    WindowInstance instance;
    instance.Instantiate(*definition);
    instance.Create(*variant, ! hardware);
    HMONITOR monitor = MonitorFromWindow(instance.Hwnd(), MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    static_cast<void>(GetMonitorInfoW(monitor, &monitorInfo));
    RECT windowRect{};
    static_cast<void>(GetWindowRect(instance.Hwnd(), &windowRect));
    const bool monitorOnAdapter = std::find(adapterMonitors.begin(), adapterMonitors.end(), monitor) != adapterMonitors.end();

    PresentFrameMetrics metrics;
    DxUi::SetFrameMetricSink(&CollectPresentFrameMetric, &metrics);
    const auto clearSink           = wil::scope_exit([] { DxUi::SetFrameMetricSink(nullptr, nullptr); });
    constexpr size_t kWarmupFrames = 60u;
    const auto frame               = [&]
    {
        instance.Host().Invalidate();
        instance.Frame();
    };
    for (size_t index = 0u; index < kWarmupFrames; ++index)
    {
        frame();
        static_cast<void>(DwmFlush());
    }
    std::vector<uint64_t> cycles, renderUs, presentUs, totalUs, presents, renders;
    using Clock = std::chrono::steady_clock;
    LARGE_INTEGER qpc0{}, qpc1{};
    static_cast<void>(QueryPerformanceCounter(&qpc0));
    const uint64_t ticks0 = NativeWorkTests::ReadCallTicks();
    const auto started    = Clock::now();
    size_t missingMetrics = 0u;
    for (;;)
    {
        const double elapsed = std::chrono::duration<double>(Clock::now() - started).count();
        if (cycles.size() >= minimumFrames && elapsed * 1000.0 >= static_cast<double>(milliseconds))
            break;
        metrics.frames = 0u;
        const DxUi::Detail::WorkCounterScope scope;
        const uint64_t before = PresentThreadCycles();
        frame();
        cycles.push_back(PresentThreadCycles() - before);
        const DxUi::Detail::WorkCounters work = scope.Delta();
        missingMetrics += metrics.frames == 1u ? 0u : 1u;
        renderUs.push_back(metrics.renderUs);
        presentUs.push_back(metrics.presentUs);
        totalUs.push_back(metrics.totalUs);
        presents.push_back(work.frame.presents);
        renders.push_back(work.frame.renders);
        static_cast<void>(DwmFlush());
    }
    const double seconds = std::chrono::duration<double>(Clock::now() - started).count();
    static_cast<void>(QueryPerformanceCounter(&qpc1));
    const uint64_t ticks1 = NativeWorkTests::ReadCallTicks();
    LARGE_INTEGER frequency{};
    static_cast<void>(QueryPerformanceFrequency(&frequency));
    const double clockSeconds = static_cast<double>(qpc1.QuadPart - qpc0.QuadPart) / static_cast<double>(frequency.QuadPart);
    ShowWindow(instance.Hwnd(), SW_HIDE);

    const std::filesystem::path path(argv[2]);
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream output{path, std::ios::binary};
    Require(bool(output), "present attribution report file");
    output << std::setprecision(10) << "{\"schema\":1,\"method\":\"dxui-frame-attribution-v1\",\"pass\":\"present\",\"fixture\":\"" << definition->id
           << (variant == &definition->variants.front() ? std::string() : '@' + std::string(variant->name)) << "\",\"width\":" << variant->width
           << ",\"height\":" << variant->height << ",\"dpi\":" << variant->dpi << ",\"renderer\":\"" << (hardware ? "Hardware" : "WARP")
           << "\",\"adapter\":{\"description\":";
    WritePresentJsonString(output, adapterName);
    output << ",\"vendorId\":" << vendorId << ",\"deviceId\":" << deviceId << ",\"driverVersion\":\"" << (driver >> 48u) << '.' << ((driver >> 32u) & 0xFFFFu)
           << '.' << ((driver >> 16u) & 0xFFFFu) << '.' << (driver & 0xFFFFu) << "\",\"selection\":\"default adapter (the WindowHost names none)\"}"
           << ",\"window\":{\"left\":" << windowRect.left << ",\"top\":" << windowRect.top << ",\"nonActivating\":true,\"monitor\":";
    WritePresentJsonString(output, monitorInfo.szDevice);
    output << ",\"monitorOnAdapter\":" << (monitorOnAdapter ? "true" : "false") << "},\"presentation\":\"HwndSwapChain, Present(1, 0)\""
           << ",\"pacing\":\"DwmFlush after every frame\",\"warmupFrames\":" << kWarmupFrames << ",\"frames\":" << cycles.size()
           << ",\"elapsedSeconds\":" << seconds << ",\"minimumFrames\":" << minimumFrames
           << ",\"minimumSeconds\":" << static_cast<double>(milliseconds) / 1000.0 << ",\"framesWithoutMetrics\":" << missingMetrics << ",\"tickSource\":\""
#if defined(_M_X64)
           << "rdtsc"
#else
           << "QueryPerformanceCounter"
#endif
           << "\",\"ticksPerSecond\":" << (clockSeconds > 0.0 ? static_cast<double>(ticks1 - ticks0) / clockSeconds : 0.0) << ",\"samples\":{";
    const auto series = [&](const char* name, const std::vector<uint64_t>& values, bool first)
    {
        output << (first ? "" : ",") << '"' << name << "\":[";
        for (size_t index = 0u; index < values.size(); ++index)
            output << (index ? "," : "") << values[index];
        output << ']';
    };
    series("frameCycles", cycles, true);
    series("renderUs", renderUs, false);
    series("presentUs", presentUs, false);
    series("totalUs", totalUs, false);
    series("presents", presents, false);
    series("renders", renders, false);
    output << "}}\n";
    output.close();
    Require(bool(output), "present attribution report written");
    return 0;
}
