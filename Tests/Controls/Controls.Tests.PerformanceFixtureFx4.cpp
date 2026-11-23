// Performance fixture FX4 (plan W0.4): the runner of the interactive-only menu fixture whose definition and phases are in
// Tests/Embedded/Embedded.Tests.PerformanceFixtureFx4.h. Tests/README.md, "Performance fixtures"; run sheet:
// Measurements/PerformanceAndMotion_2026-11-13/FX4/README.md. Never run outside the interactive desktop lease.
//
// The scene is an embedded-only application: on this thread, an EmbeddedHost attached to the application's own WARP device
// is the only DxUi host, and a plain Win32 window owns the menus. No native WindowHost is attached, so each menu session
// brings up, and its close tears down, the thread's shared native device stack (G10).
//
// A step is one action and then a pump of this thread's queue: every posted and sent message other than timers is
// dispatched, so the popup's WM_PAINT renders it, exactly once per invalidation. Timers stay queued (they depend on the
// real clock). The counters of a step are those of the action and its pump, after a ResourceLifetimeTracker sweep.
//
// Independent of the real pointer:
// - every menu opens with its first row selected (ContextMenuSessionCallbacks::focusFirstNavigableItem), so the popup
//   never seeds a hovered row from GetCursorPos;
// - the application window, and so the menu, is placed in the half of the primary monitor's work area that does not hold
//   the pointer (read once with GetCursorPos; the fixture never moves the pointer), and a menu that opens under the
//   pointer ends the run as invalid;
// - every step is a key sent to the popup (SendMessageW WM_KEYDOWN, as the Menu and MenuResources suites do), and after
//   every keyboard step the popup must report no hovered row and the expected keyboard row: any mouse or keyboard input
//   from the person reaching the menu ends the run as invalid.
//
// Blocked activation: the testing contract says a blocked activation path can leave a temporary 1-by-1 popup despite
// correct internal layout. After every open the popup must be larger than 1 by 1 and match its own layout rectangle, be the
// foreground window, hold keyboard focus and hold mouse capture; the application window must first take the foreground.
// Otherwise the run fails with that reason and no receipt is written.
#include "Controls.Tests.PerformanceFixtureFx4.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "GraphicsFixture.h"
#include "Support/AnimationDispatcher.h"
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // definitions are never copied
#include "Embedded.Tests.PerformanceFixtureFx4.h"
#pragma warning(pop)

#include <psapi.h>

#include <format>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#pragma comment(lib, "psapi.lib")

namespace
{
using namespace PerformanceFixtures;

constexpr wchar_t kOwnerClass[]     = L"DxUiPerformanceFixtureFx4Application";
constexpr wchar_t kMenuPopupClass[] = L"DxUi_ContextMenu";
constexpr int kOwnerWidthPx         = 240;
constexpr int kOwnerHeightPx        = 40;
constexpr int kWorkAreaMarginPx     = 32;

[[nodiscard]] PROCESS_MEMORY_COUNTERS_EX Fx4ProcessMemory() noexcept
{
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    static_cast<void>(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)));
    return memory;
}

// The adapter a hardware device creation without an explicit adapter uses: the popups' shared native device stack tries it
// first and falls back to WARP only when it fails.
[[nodiscard]] AdapterRecord DescribeDefaultAdapter()
{
    AdapterRecord record{"default adapter (not readable)", 0u, 0u, false, 0u, 0u};
    wil::com_ptr_nothrow<IDXGIFactory1> factory;
    wil::com_ptr_nothrow<IDXGIAdapter1> adapter;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.put()))) || FAILED(factory->EnumAdapters1(0u, adapter.put())))
        return record;
    DXGI_ADAPTER_DESC1 description{};
    if (FAILED(adapter->GetDesc1(&description)))
        return record;
    record.description.clear();
    for (const wchar_t* character = description.Description; *character != L'\0'; ++character)
        record.description.push_back(*character < 0x80 ? static_cast<char>(*character) : '?');
    record.description += " (popup windows: the thread's shared native device stack, hardware first, WARP fallback)";
    record.vendorId   = description.VendorId;
    record.deviceId   = description.DeviceId;
    record.software   = (description.Flags & static_cast<UINT>(DXGI_ADAPTER_FLAG_SOFTWARE)) != 0u;
    record.videoBytes = description.DedicatedVideoMemory;
    LARGE_INTEGER driver{};
    if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver)))
        record.driver = static_cast<uint64_t>(driver.QuadPart);
    return record;
}

LRESULT CALLBACK Fx4OwnerProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

[[nodiscard]] bool RegisterFx4OwnerClass() noexcept
{
    static const bool registered = []() noexcept
    {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize        = sizeof(windowClass);
        windowClass.lpfnWndProc   = Fx4OwnerProc;
        windowClass.hInstance     = GetModuleHandleW(nullptr);
        windowClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
        windowClass.lpszClassName = kOwnerClass;
        return RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }();
    return registered;
}

std::vector<DxUi::MenuFlyoutItem> MakeRows(size_t count, std::wstring_view prefix)
{
    std::vector<DxUi::MenuFlyoutItem> rows;
    rows.reserve(count);
    for (size_t index = 0u; index < count; ++index)
        rows.push_back(DxUi::MenuFlyoutItem{
            .kind = DxUi::MenuItemKind::Standard, .text = std::format(L"{} {:04}", prefix, index + 1u), .commandId = static_cast<int>(index + 1u)});
    return rows;
}

// The embedded-only application and its menu sessions. Every failure is a sentence saying why the run is invalid.
class Fx4Application final
{
public:
    Fx4Application()                                 = default;
    Fx4Application(const Fx4Application&)            = delete;
    Fx4Application& operator=(const Fx4Application&) = delete;
    Fx4Application(Fx4Application&&)                 = delete;
    Fx4Application& operator=(Fx4Application&&)      = delete;
    ~Fx4Application()
    {
        Abort();
        view_.reset();
    }

    [[nodiscard]] std::string Build(const Definition& definition, const Variant& variant)
    {
        if (! RegisterFx4OwnerClass())
            return "the application window class could not be registered";
        if (FAILED(gpu_.Create(true)) || FAILED(DxUi::GraphicsDevice::Create(gpu_.device.get(), graphics_)))
            return "the application's WARP device could not be created";
        theme_               = DxUi::MakeDefaultThemePalette(true);
        theme_.reducedMotion = definition.reducedMotion;
        view_                = std::make_unique<DxUi::EmbeddedHost>();
        if (FAILED(view_->Attach(graphics_, StoppedTestClock())))
            return "the application's EmbeddedHost could not attach";
        view_->Controls().SetTheme(theme_);
        auto root = std::make_unique<DxUi::Panel>();
        root->AddChild<DxUi::Label>(L"Embedded-only application")->SetBounds(D2D1::RectF(16.0f, 16.0f, 400.0f, 48.0f));
        view_->Controls().SetRoot(std::move(root));
        if (FAILED(PrepareEmbeddedContentForTests(*view_, variant.width, variant.height, variant.dpi)))
            return "the application's view could not be prepared";
        rows_        = MakeRows(Fx4::kRows, L"Row");
        sessionRows_ = MakeRows(Fx4::kSessionRows, L"Session item");

        // The half of the primary monitor's work area that does not hold the pointer. The pointer is only read, never moved.
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        if (GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitor) == FALSE)
            return "the primary monitor's work area could not be read";
        POINT pointer{};
        static_cast<void>(GetCursorPos(&pointer)); // getcursorpos-allow: choose the half of the work area away from the pointer
        const RECT& work   = monitor.rcWork;
        const LONG centerX = work.left + (work.right - work.left) / 2;
        const LONG left    = pointer.x < centerX ? centerX + kWorkAreaMarginPx : work.left + kWorkAreaMarginPx;
        const LONG top     = work.top + kWorkAreaMarginPx;
        owner_.reset(CreateWindowExW(WS_EX_TOOLWINDOW,
                                     kOwnerClass,
                                     L"DxUi FX4 embedded-only application",
                                     WS_POPUP | WS_BORDER,
                                     left,
                                     top,
                                     kOwnerWidthPx,
                                     kOwnerHeightPx,
                                     nullptr,
                                     nullptr,
                                     GetModuleHandleW(nullptr),
                                     nullptr));
        if (! owner_)
            return "the application window could not be created";
        anchor_ = POINT{left, top + kOwnerHeightPx};
        ShowWindow(owner_.get(), SW_SHOWNOACTIVATE);
        Pump();
        const char* stage = "";
        if (! TryActivateDxUiTestWindow(owner_.get(), 2000u, &stage))
            return std::format("blocked activation: the application window could not take the foreground (stage {}); FX4 runs only in an "
                               "interactive run under the desktop lease",
                               stage);
        return {};
    }

    // Applies step `step` of phase `phase` and pumps. Only Win32 state is read here: nothing DxUi counts.
    [[nodiscard]] std::string Step(size_t phase, size_t step)
    {
        switch (phase)
        {
            case Fx4::kWarm:
            case Fx4::kCycle: return step == 0u ? Open(rows_) : Close();
            case Fx4::kOpen: return Open(rows_);
            case Fx4::kKeyboard: return Key(VK_DOWN);
            case Fx4::kClose: return Close();
            case Fx4::kSession:
            {
                std::string failure = Open(sessionRows_);
                return failure.empty() ? Close() : failure;
            }
            default: return "the step names no FX4 phase";
        }
    }

    // After a step's counters are taken: the popup's own state, read through its diagnostics.
    [[nodiscard]] std::string Verify(size_t phase, size_t step)
    {
        if (phase != Fx4::kOpen && phase != Fx4::kKeyboard && ! (phase == Fx4::kCycle && step == 0u) && ! (phase == Fx4::kWarm && step == 0u))
            return {};
        DxUi::ContextMenuPopupDebugState state{};
        if (! DxUi::DebugGetContextMenuPopupState(popup_, state))
            return "the popup's state could not be read";
        RECT window{};
        static_cast<void>(GetWindowRect(popup_, &window));
        if (window.left != state.windowRectPx.left || window.top != state.windowRectPx.top || window.right != state.windowRectPx.right ||
            window.bottom != state.windowRectPx.bottom)
            return std::format("blocked activation: the popup window ({},{})-({},{}) is not its layout rectangle ({},{})-({},{})",
                               window.left,
                               window.top,
                               window.right,
                               window.bottom,
                               state.windowRectPx.left,
                               state.windowRectPx.top,
                               state.windowRectPx.right,
                               state.windowRectPx.bottom);
        if (state.hoveredIndex)
            return std::format("the pointer reached the menu: row {} is hovered", *state.hoveredIndex);
        const size_t expected = phase == Fx4::kKeyboard ? step + 1u : 0u;
        if (state.keyboardIndex != std::optional<size_t>(expected))
            return std::format("input other than the fixture's reached the menu: the keyboard row is {}, expected {}",
                               state.keyboardIndex ? std::to_string(*state.keyboardIndex) : std::string("none"),
                               expected);
        popupDpi_ = state.dpi;
        return {};
    }

    void Hide() noexcept
    {
        view_->SetVisible(false);
    }
    [[nodiscard]] UINT PopupDpi() const noexcept
    {
        return popupDpi_;
    }
    [[nodiscard]] RECT PopupRect() const noexcept
    {
        return popupRect_;
    }

    // Leaves the desktop as the run found it as far as this application can: any open menu is closed.
    void Abort() noexcept
    {
        if (popup_ && IsWindow(popup_) != FALSE && ! closed_)
        {
            static_cast<void>(SendMessageW(popup_, WM_KEYDOWN, VK_ESCAPE, 1));
            Pump();
        }
        popup_ = nullptr;
    }

private:
    struct PopupSearch
    {
        HWND owner = nullptr;
        HWND popup = nullptr;
    };

    static BOOL CALLBACK FindPopupWindow(HWND hwnd, LPARAM parameter) noexcept
    {
        auto& search = *reinterpret_cast<PopupSearch*>(parameter);
        wchar_t className[64]{};
        if (GetWindow(hwnd, GW_OWNER) == search.owner && GetClassNameW(hwnd, className, static_cast<int>(std::size(className))) > 0 &&
            std::wstring_view(className) == kMenuPopupClass)
        {
            search.popup = hwnd;
            return FALSE;
        }
        return TRUE;
    }

    // Every posted and sent message but timers, until the queue is empty: the popup's WM_PAINT renders it here.
    static void Pump() noexcept
    {
        constexpr UINT kSystemTimer = 0x0118u;
        MSG message{};
        for (size_t count = 0u; count < 4096u; ++count)
        {
            if (! PeekMessageW(&message, nullptr, 0u, WM_TIMER - 1u, PM_REMOVE) && ! PeekMessageW(&message, nullptr, kSystemTimer + 1u, 0xFFFFu, PM_REMOVE))
                break;
            static_cast<void>(DispatchQueuedMessageForTest(message));
        }
    }

    [[nodiscard]] std::string Open(std::span<const DxUi::MenuFlyoutItem> items)
    {
        if (popup_)
            return "a menu was already open";
        DxUi::ContextMenuSessionCallbacks callbacks{};
        callbacks.focusFirstNavigableItem = true;
        callbacks.maxRootHeightDip        = Fx4::kMaxHeightDip;
        callbacks.minRootWidthDip         = Fx4::kMinWidthDip;
        closed_                           = false;
        if (! DxUi::ContextMenu::ShowAsync(owner_.get(), anchor_, items, theme_, [this](std::optional<int>) noexcept { closed_ = true; }, callbacks))
        {
            closed_ = true;
            return "the menu did not open";
        }
        PopupSearch search{owner_.get(), nullptr};
        static_cast<void>(EnumThreadWindows(GetCurrentThreadId(), FindPopupWindow, reinterpret_cast<LPARAM>(&search)));
        popup_ = search.popup;
        if (! popup_)
            return "the menu's popup window was not found";
        Pump();
        return CheckActivated();
    }

    [[nodiscard]] std::string CheckActivated()
    {
        if (closed_ || IsWindow(popup_) == FALSE)
            return "the menu was dismissed as it opened: another window took the foreground";
        RECT window{};
        static_cast<void>(GetWindowRect(popup_, &window));
        popupRect_ = window;
        if (window.right - window.left <= 1 || window.bottom - window.top <= 1)
            return std::format("blocked activation: the popup window is {} by {} pixels", window.right - window.left, window.bottom - window.top);
        if (GetForegroundWindow() != popup_)
            return "blocked activation: the popup is not the foreground window";
        if (GetFocus() != popup_)
            return "blocked activation: the popup does not hold keyboard focus";
        if (GetCapture() != popup_)
            return "blocked activation: the popup does not hold mouse capture";
        POINT pointer{};
        if (GetCursorPos(&pointer) != FALSE && PtInRect(&window, pointer) != FALSE) // getcursorpos-allow: the menu must not open under the pointer
            return "the pointer rests where the menu opened; move it to the other half of the primary monitor and run again";
        return {};
    }

    [[nodiscard]] std::string Key(UINT virtualKey)
    {
        if (! popup_ || closed_ || IsWindow(popup_) == FALSE)
            return "the menu was dismissed during the keyboard steps: another window took the foreground";
        static_cast<void>(SendMessageW(popup_, WM_KEYDOWN, virtualKey, 1));
        Pump();
        if (closed_ || GetForegroundWindow() != popup_)
            return "the menu lost the foreground during the keyboard steps";
        return {};
    }

    [[nodiscard]] std::string Close()
    {
        if (! popup_ || closed_ || IsWindow(popup_) == FALSE)
            return "the menu was dismissed before the fixture closed it: another window took the foreground";
        static_cast<void>(SendMessageW(popup_, WM_KEYDOWN, VK_ESCAPE, 1));
        Pump();
        const HWND closing = popup_;
        popup_             = nullptr;
        if (! closed_ || IsWindow(closing) != FALSE)
            return "Escape did not end the menu session";
        return {};
    }

    GraphicsFixture gpu_;
    std::shared_ptr<DxUi::GraphicsDevice> graphics_;
    std::unique_ptr<DxUi::EmbeddedHost> view_;
    DxUi::ThemePalette theme_{};
    wil::unique_hwnd owner_;
    std::vector<DxUi::MenuFlyoutItem> rows_;
    std::vector<DxUi::MenuFlyoutItem> sessionRows_;
    POINT anchor_{};
    HWND popup_     = nullptr;
    bool closed_    = true;
    UINT popupDpi_  = 0u;
    RECT popupRect_ = {};
};

struct Fx4Run
{
    RunRecord run;
    std::string failure; // empty: a valid run
    UINT popupDpi = 0u;
    RECT popupRect{};
};

// One run: counters per step as RunWindow records them, memory at the device, after the build, after each phase and hidden.
Fx4Run RunFx4(const Definition& definition, const Variant& variant)
{
    Fx4Run result;
    RunRecord& run            = result.run;
    run.definition            = &definition;
    run.variant               = &variant;
    run.host                  = Host::Window;
    run.cppAllocationsCounted = false;
    run.adapter               = DescribeDefaultAdapter();
    run.hardware              = ! run.adapter.software;
    run.entryPrivateBytes     = Fx4ProcessMemory().PrivateUsage;
    run.phases.resize(definition.phases.size());
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
    {
        run.phases[phase].index = phase;
        run.phases[phase].steps.reserve(definition.phases[phase].steps);
    }
    run.memory.reserve(definition.phases.size() + 3u);
    const auto sample = [&](std::string_view at, std::string_view phase)
    {
        const auto process = Fx4ProcessMemory();
        MemorySample value{};
        value.at              = at;
        value.phase           = phase;
        value.privateBytes    = process.PrivateUsage;
        value.workingSetBytes = process.WorkingSetSize;
        run.memory.push_back(value);
    };
    std::vector<DxUi::Detail::TrackedResource> storage(262144u);
    sample("device", {});
    Fx4Application application;
    result.failure = application.Build(definition, variant);
    if (! result.failure.empty())
        return result;
    sample("built", {});
    {
        DxUi::Detail::ResourceLifetimeTracker tracker(storage);
        for (size_t phase = 0u; phase < definition.phases.size() && result.failure.empty(); ++phase)
        {
            NoteDxUiTestProgress(); // the watchdog bounds a phase, not both runs of the gate
            for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
            {
                const DxUi::Detail::WorkCounterScope scope;
                std::string failure = application.Step(phase, step);
                static_cast<void>(tracker.Sweep());
                run.phases[phase].steps.push_back(StepRecord{scope.Delta(), 0u});
                if (failure.empty())
                    failure = application.Verify(phase, step);
                if (! failure.empty())
                {
                    result.failure = std::format("phase {} step {}: {}", definition.phases[phase].name, step, failure);
                    break;
                }
            }
            if (result.failure.empty())
                sample("phase", definition.phases[phase].name);
        }
        application.Abort();
        run.trackedResources = tracker.Held();
        run.trackerOverflow  = tracker.Overflow();
    }
    if (! result.failure.empty())
        return result;
    application.Hide();
    sample("hidden", {});
    result.popupDpi  = application.PopupDpi();
    result.popupRect = application.PopupRect();
    return result;
}

void PrintFx4Summary(const Fx4Run& result)
{
    const RunRecord& run = result.run;
    const std::string line =
        std::format("FX4 (interactive): popup DPI {}, last popup {}x{} px; open: {} popup paints, {} backdrop captures; keyboard step: {}-{} popup "
                    "paints, {}-{} rows painted, {}-{} effects created; sessions: {} D3D11 devices, {} composition devices in {} sessions\n",
                    result.popupDpi,
                    result.popupRect.right - result.popupRect.left,
                    result.popupRect.bottom - result.popupRect.top,
                    Summarize(run.Phase("open"), "popups.menuPopupPaints").total,
                    Summarize(run.Phase("open"), "popups.backdropCaptures").total,
                    Summarize(run.Phase("keyboard"), "popups.menuPopupPaints").minimum,
                    Summarize(run.Phase("keyboard"), "popups.menuPopupPaints").maximum,
                    Summarize(run.Phase("keyboard"), "popups.menuRowsPainted").minimum,
                    Summarize(run.Phase("keyboard"), "popups.menuRowsPainted").maximum,
                    Summarize(run.Phase("keyboard"), "resources.effect.created").minimum,
                    Summarize(run.Phase("keyboard"), "resources.effect.created").maximum,
                    Summarize(run.Phase("sessions"), "resources.d3d11Device.created").total,
                    Summarize(run.Phase("sessions"), "frame.compositionDevices").total,
                    Fx4::kSessions);
    std::cout << line;
}

// A run measured something only if every keyboard step rendered the popup.
[[nodiscard]] std::string CheckFx4Measured(const Fx4Run& result)
{
    if (result.run.trackerOverflow != 0u)
        return "the tracker did not observe every resource FX4 created";
    if (Summarize(result.run.Phase("keyboard"), "popups.menuPopupPaints").minimum == 0u)
        return "a keyboard step did not render the popup";
    return {};
}
} // namespace

void TestPerformanceFixtureFx4IsDeterministic()
{
    const Variant& variant = Fx4::kVariants[0];
    const Fx4Run first     = RunFx4(Fx4::kDefinition, variant);
    if (! CheckDxUiTestResult(first.failure.empty(), ("FX4 is invalid, first run, " + first.failure).c_str()))
        return;
    const Fx4Run second = RunFx4(Fx4::kDefinition, variant);
    if (! CheckDxUiTestResult(second.failure.empty(), ("FX4 is invalid, second run, " + second.failure).c_str()))
        return;
    const std::string measured = CheckFx4Measured(first);
    if (! CheckDxUiTestResult(measured.empty(), ("FX4: " + measured).c_str()))
        return;
    if (const std::optional<Difference> difference = FirstDifference(first.run, second.run))
    {
        static_cast<void>(CheckDxUiTestResult(false, ("Nondeterministic performance fixture: " + Describe(first.run, *difference)).c_str()));
        return;
    }
    PrintFx4Summary(first);
}

int RunInteractiveFixtureReceipt(const Definition& definition, const Variant& variant, const std::filesystem::path& output)
{
    if (&definition != &Fx4::kDefinition)
    {
        std::fputs("--fixture-receipt --interactive names no interactive-only fixture with a runner.\n", stderr);
        return 2;
    }
    const Fx4Run result = RunFx4(definition, variant);
    DxUi::Ui::AnimationDispatcher::GetInstance().Shutdown();
    std::string failure = result.failure.empty() ? CheckFx4Measured(result) : result.failure;
    if (! failure.empty())
    {
        const std::string message = "FX4 receipt not written: the run is invalid: " + failure + '\n';
        std::cerr << message;
        return 3;
    }
    PrintFx4Summary(result);
    if (output.has_parent_path())
        std::filesystem::create_directories(output.parent_path());
    std::ofstream file{output, std::ios::binary};
    Require(bool(file), "performance fixture receipt file");
    WriteReceiptJson(file, result.run);
    file.close();
    Require(bool(file), "performance fixture receipt written");
    return 0;
}
