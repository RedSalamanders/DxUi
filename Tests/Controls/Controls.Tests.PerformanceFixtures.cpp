// Performance fixtures on the WindowHost: the entry point of DxUi.ControlTests.exe (plan W0.4; Tests/README.md,
// "Performance fixtures"). Fixtures are defined in Tests/Embedded/Embedded.Tests.PerformanceFixtureFx<N>.h and listed in
// Embedded.Tests.PerformanceFixtureCatalog.h; this file runs the ones whose hosts include the window host. A window frame
// is the host's own WM_PAINT: posted messages other than timers are pumped, then UpdateWindow paints whatever is invalid.
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

#include <psapi.h>

#include <fstream>
#include <sstream>

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
    void Instantiate(const Definition& definition)
    {
        definition_ = &definition;
        scenario_   = definition.create();
    }
    void Create(const Variant& variant)
    {
        window_.emplace(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
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
                continue;
            static_cast<void>(DispatchQueuedMessageForTest(message));
        }
        static_cast<void>(UpdateWindow(window_->Hwnd()));
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
        static_cast<void>(window_->Host().DebugAnimationTickForTest(now_));
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
    const Definition* definition_ = nullptr;
    // Declared before the window it may own models for, so the window and its host are destroyed first.
    std::unique_ptr<Scenario> scenario_;
    std::optional<AttachedHostWindow> window_;
    std::optional<Context> context_;
    uint64_t now_ = kClockOriginMs;
};

[[nodiscard]] PROCESS_MEMORY_COUNTERS_EX ProcessMemory()
{
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    Require(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE, "process memory");
    return memory;
}

// One run on the window host, as RunEmbedded does on the embedded host (counters per step after a tracker sweep, memory
// per phase), without the C++ allocation sink, retained statistics or video memory, which this host does not report.
RunRecord RunWindow(const Definition& definition, const Variant& variant)
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
                const DxUi::Detail::WorkCounterScope scope;
                instance.Step(phase, step);
                instance.Frame();
                static_cast<void>(tracker.Sweep());
                run.phases[phase].steps.push_back(StepRecord{scope.Delta(), 0u});
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

void RequireDeterministic(const RunRecord& first, const RunRecord& second)
{
    if (const std::optional<Difference> difference = FirstDifference(first, second))
    {
        std::cerr << "Nondeterministic performance fixture: " << Describe(first, *difference) << '\n';
        Require(false, "two consecutive WARP runs of a performance fixture record equal counters");
    }
}
} // namespace
#pragma warning(pop)

void TestPerformanceFixtureFrameworkOnWindowHost()
{
    const Definition& definition = SelfTest::kDefinition;
    const RunRecord first        = RunWindow(definition, SelfTest::kVariants[0]);
    const RunRecord second       = RunWindow(definition, SelfTest::kVariants[0]);
    RequireDeterministic(first, second);
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
            RequireDeterministic(RunWindow(*definition, variant), RunWindow(*definition, variant));
            std::cerr << "    [FIXTURE] " << definition->id << '@' << variant.name << " (window, WARP): two runs record equal counters ("
                      << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count() << " ms)\n";
        }
    }
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
