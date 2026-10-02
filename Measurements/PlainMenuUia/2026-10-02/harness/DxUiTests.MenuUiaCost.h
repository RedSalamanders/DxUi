#pragma once

// Retained evaluation harness: what per-entry UI Automation elements cost menus without descriptions
// (Measurements/PlainMenuUia/2026-10-02). It is not a suite of the repository; Set-Harness.ps1 switches it on in a
// checkout (included after DxUiTests.MenuResources.h, registered as the fixture suite MenuUiaCost).
//   DxUi.ControlTests.exe --suite=MenuUiaCost --no-activate
// It opens menus from an owned, nonactivating window and drives their popups with sent messages: nothing takes the
// foreground, the keyboard focus or the pointer, and every popup closes by Escape before the next opens. Each line of
// output is one JSON object of fixture dxui-menu-uia-cost-v1. DXUI_MENU_UIA_CYCLES (default 24) sets the memory cycles
// and DXUI_MENU_UIA_OPENS (default 8) the timed openings of each timed menu.
#include <iomanip>

namespace
{
[[nodiscard]] int MenuUiaCostSetting(const wchar_t* name, int fallback)
{
    wchar_t buffer[32]{};
    const DWORD length = GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0u || length >= std::size(buffer))
        return fallback;
    const int value = _wtoi(buffer);
    return value > 0 ? value : fallback;
}

struct MenuUiaCostVariant
{
    const char* name;
    size_t rows;
    DxUi::MenuItemKind kind;
    bool distinct;
    bool described;
};

[[nodiscard]] std::vector<DxUi::MenuFlyoutItem> BuildMenuUiaCostItems(const MenuUiaCostVariant& variant)
{
    using namespace DxUi;
    std::vector<MenuFlyoutItem> items;
    items.reserve(variant.rows);
    for (size_t row = 0; row < variant.rows; ++row)
    {
        MenuFlyoutItem item{.kind      = variant.kind,
                            .text      = variant.distinct ? std::format(L"Archives photographiques {} de la réunion familiale", row)
                                                          : std::wstring(L"Archives photographiques de la réunion familiale"),
                            .commandId = static_cast<int>(9000u + row)};
        if (variant.described)
            item.secondaryText = std::format(L"D:\\Sauvegardes\\Collection déjà présente\\Génération {}", row);
        items.push_back(std::move(item));
    }
    return items;
}

// Wall time and the calling thread's own CPU cycles: a paint's Present waits for a vertical blank once the swap chain's
// queue is full, which the wall time includes and the cycles do not.
struct MenuUiaCostClock
{
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    ULONG64 cycles                                = 0u;

    MenuUiaCostClock() noexcept
    {
        static_cast<void>(QueryThreadCycleTime(GetCurrentThread(), &cycles));
    }

    void Stop(std::vector<double>& wallUs, std::vector<double>& kiloCycles) const
    {
        ULONG64 now = 0u;
        static_cast<void>(QueryThreadCycleTime(GetCurrentThread(), &now));
        wallUs.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count());
        kiloCycles.push_back(static_cast<double>(now - cycles) / 1000.0);
    }
};

struct MenuUiaCostSeries
{
    std::vector<double> wallUs;
    std::vector<double> kiloCycles;
};

void WriteMenuUiaCostSeries(const char* variant, int open, const char* operation, const MenuUiaCostSeries& series)
{
    const auto write = [](const std::vector<double>& values)
    {
        std::cout << '[';
        for (size_t index = 0; index < values.size(); ++index)
            std::cout << (index ? "," : "") << std::fixed << std::setprecision(2) << values[index];
        std::cout << std::defaultfloat << ']';
    };
    std::cout << "{\"fixture\":\"dxui-menu-uia-cost-v1\",\"part\":\"timing\",\"variant\":\"" << variant << "\",\"open\":" << open << ",\"operation\":\""
              << operation << "\",\"wallUs\":";
    write(series.wallUs);
    std::cout << ",\"kiloCycles\":";
    write(series.kiloCycles);
    std::cout << "}\n";
}
} // namespace

void RunMenuUiaCostTests()
{
    using namespace DxUi;
    AttachedHostWindow owner;
    // One fixed monitor, as MenuResourceScaling pins it: an anchor clamped from the helper's offscreen position could
    // land on a monitor of another DPI and change the surface between runs.
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    Require(GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitorInfo) != FALSE, "cost fixture resolves its fixed monitor");
    Require(SetWindowPos(owner.Hwnd(), nullptr, monitorInfo.rcWork.left + 64, monitorInfo.rcWork.top + 64, 320, 200, SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
            "cost fixture positions its owned nonactivating window");
    ShowWindow(owner.Hwnd(), SW_SHOWNOACTIVATE);
    owner.PumpMessages();
    const POINT anchor = ClientScreenPointForTest(owner.Hwnd(), 20, 20, "cost fixture anchor maps to screen");
    ContextMenuSessionCallbacks callbacks{};
    callbacks.maxRootHeightDip = 300.0f;
    callbacks.minRootWidthDip  = 456.0f;
    std::cout << "{\"fixture\":\"dxui-menu-uia-cost-v1\",\"part\":\"environment\",\"uiaClientsListening\":" << (UiaClientsAreListening() ? "true" : "false")
              << ",\"menuItemObjectBytes\":" << sizeof(MenuFlyoutItem) << ",\"labelObjectBytes\":" << sizeof(Label)
              << ",\"toggleObjectBytes\":" << sizeof(Toggle) << "}\n";

    const auto open = [&](const std::vector<MenuFlyoutItem>& items, bool& closed)
    {
        Require(ContextMenu::ShowAsync(owner.Hwnd(), anchor, items, owner.Host().GetTheme(), [&closed](std::optional<int>) noexcept { closed = true; }, callbacks),
                "cost fixture menu opens");
    };
    const auto find = [&](const std::vector<MenuFlyoutItem>& items)
    {
        const HWND popup = WaitForOwnedContextMenuPopupWindowByFirstItemText(owner.Hwnd(), items.front().text);
        Require(popup != nullptr, "cost fixture menu belongs to the fixture");
        return popup;
    };
    const auto close = [&](HWND popup, const bool& closed)
    {
        SendMessageW(popup, WM_KEYDOWN, VK_ESCAPE, 0);
        owner.PumpMessages();
        Require(closed && WaitForWindowDestroyed(popup), "cost fixture menu closes");
    };
    // Even cycles walk the variants forward and odd ones backward, each from another start, as MenuRowLiveBytes did.
    const auto rotate = [](int cycle, size_t step, size_t count)
    { return (static_cast<size_t>(cycle) + (cycle % 2 == 0 ? step : count - 1u - step)) % count; };

    // Memory: live heap bytes (busy blocks of every process heap), private bytes and the menu resources the library
    // counts, before an opening, once the open menu has painted and after it closed.
    const std::array<MenuUiaCostVariant, 9> memoryVariants{{{"radio-12", 12u, MenuItemKind::Radio, false, false},
                                                            {"radio-24", 24u, MenuItemKind::Radio, false, false},
                                                            {"radio-48", 48u, MenuItemKind::Radio, false, false},
                                                            {"standard-12", 12u, MenuItemKind::Standard, true, false},
                                                            {"standard-48", 48u, MenuItemKind::Standard, true, false},
                                                            {"standard-128", 128u, MenuItemKind::Standard, true, false},
                                                            {"standard-512", 512u, MenuItemKind::Standard, true, false},
                                                            {"standard-4096", 4096u, MenuItemKind::Standard, true, false},
                                                            {"described-12", 12u, MenuItemKind::Radio, false, true}}};
    const int cycles  = MenuUiaCostSetting(L"DXUI_MENU_UIA_CYCLES", 24);
    const auto sample = [&](const MenuUiaCostVariant& variant, int cycle, const char* phase)
    {
        NoteDxUiTestProgress();
        PROCESS_MEMORY_COUNTERS_EX memory{};
        memory.cb = sizeof(memory);
        Require(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE,
                "cost fixture reads process memory");
        const uint64_t liveBytes = MeasureLiveHeapBytesForMenuSuite();
        const auto resources     = DebugGetContextMenuResources();
        std::cout << "{\"fixture\":\"dxui-menu-uia-cost-v1\",\"part\":\"memory\",\"variant\":\"" << variant.name << "\",\"cycle\":" << cycle << ",\"phase\":\""
                  << phase << "\",\"liveBytes\":" << liveBytes << ",\"privateBytes\":" << memory.PrivateUsage << ",\"popups\":" << resources.popups
                  << ",\"rowLayouts\":" << resources.rowLayouts << ",\"accessibilityRecords\":" << resources.accessibilityRecords << "}\n";
    };
    for (int cycle = 0; cycle < cycles; ++cycle)
    {
        for (size_t step = 0; step < memoryVariants.size(); ++step)
        {
            const MenuUiaCostVariant& variant       = memoryVariants[rotate(cycle, step, memoryVariants.size())];
            const std::vector<MenuFlyoutItem> items = BuildMenuUiaCostItems(variant);
            bool closed                             = false;
            sample(variant, cycle, "before");
            open(items, closed);
            const HWND popup   = find(items);
            const auto dismiss = wil::scope_exit([&]() noexcept
            {
                if (IsWindow(popup))
                    SendMessageW(popup, WM_KEYDOWN, VK_ESCAPE, 0);
            });
            Require(RedrawWindow(popup, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW) != FALSE, "cost fixture menu completes its paint");
            sample(variant, cycle, "rendered");
            close(popup, closed);
            sample(variant, cycle, "closed");
        }
    }

    // Timing: the opening (ShowAsync shows and paints the popup), then sent keys, wheel notches and pointer moves, each
    // timed alone (the handler, where the menu synchronizes its UI Automation elements) and followed by the paint it
    // asked for, timed apart; then Escape until the popup is destroyed.
    const std::array<MenuUiaCostVariant, 4> timingVariants{{{"standard-12", 12u, MenuItemKind::Standard, true, false},
                                                            {"standard-128", 128u, MenuItemKind::Standard, true, false},
                                                            {"standard-4096", 4096u, MenuItemKind::Standard, true, false},
                                                            {"described-12", 12u, MenuItemKind::Radio, false, true}}};
    const int opens = MenuUiaCostSetting(L"DXUI_MENU_UIA_OPENS", 8);
    for (int round = 0; round < opens; ++round)
    {
        for (size_t step = 0; step < timingVariants.size(); ++step)
        {
            const MenuUiaCostVariant& variant       = timingVariants[rotate(round, step, timingVariants.size())];
            const std::vector<MenuFlyoutItem> items = BuildMenuUiaCostItems(variant);
            NoteDxUiTestProgress();
            bool closed = false;
            MenuUiaCostSeries opening;
            {
                const MenuUiaCostClock clock;
                open(items, closed);
                clock.Stop(opening.wallUs, opening.kiloCycles);
            }
            const HWND popup   = find(items);
            const auto dismiss = wil::scope_exit([&]() noexcept
            {
                if (IsWindow(popup))
                    SendMessageW(popup, WM_KEYDOWN, VK_ESCAPE, 0);
            });
            Require(RedrawWindow(popup, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW) != FALSE, "timed menu completes its first paint");
            const auto drive = [&](const char* operation, int count, auto&& send)
            {
                MenuUiaCostSeries dispatch;
                MenuUiaCostSeries paint;
                for (int index = 0; index < count; ++index)
                {
                    {
                        const MenuUiaCostClock clock;
                        send(index);
                        clock.Stop(dispatch.wallUs, dispatch.kiloCycles);
                    }
                    const MenuUiaCostClock clock;
                    static_cast<void>(RedrawWindow(popup, nullptr, nullptr, RDW_UPDATENOW));
                    clock.Stop(paint.wallUs, paint.kiloCycles);
                }
                WriteMenuUiaCostSeries(variant.name, round, (std::string(operation) + "-dispatch").c_str(), dispatch);
                WriteMenuUiaCostSeries(variant.name, round, (std::string(operation) + "-paint").c_str(), paint);
            };
            WriteMenuUiaCostSeries(variant.name, round, "open", opening);

            drive("down", 32, [&](int) { SendMessageW(popup, WM_KEYDOWN, VK_DOWN, 0); });
            ContextMenuPopupDebugState state{};
            Require(DebugGetContextMenuPopupState(popup, state) && state.keyboardIndex.has_value(), "Down chooses a keyboard row");
            drive("end-home", 16, [&](int index) { SendMessageW(popup, WM_KEYDOWN, index % 2 == 0 ? VK_END : VK_HOME, 0); });
            // Home makes the first row visible, which leaves the viewport's top padding (4 DIP) scrolled away.
            Require(DebugGetContextMenuPopupState(popup, state) && state.keyboardIndex == std::optional<size_t>{0u} && state.scrollOffsetDip < 8.0f,
                    "Home returns to the first row at the top");

            const WPARAM towardUser = MAKEWPARAM(0, static_cast<WORD>(0x10000 - WHEEL_DELTA));
            const WPARAM awayFromUser = MAKEWPARAM(0, WHEEL_DELTA);
            const LPARAM wheelPoint =
                MAKELPARAM((state.surfaceRectPx.left + state.surfaceRectPx.right) / 2, (state.surfaceRectPx.top + state.surfaceRectPx.bottom) / 2);
            // Sixteen notches down and as many back: where Home left the viewport, unless the bottom clamped the descent.
            drive("wheel", 32, [&](int index) { SendMessageW(popup, WM_MOUSEWHEEL, index < 16 ? towardUser : awayFromUser, wheelPoint); });
            Require(DebugGetContextMenuPopupState(popup, state) && state.scrollOffsetDip < 8.0f, "the wheel returns to the top");

            D2D1_RECT_F first{};
            D2D1_RECT_F third{};
            Require(DebugGetContextMenuPopupItemRect(popup, 1u, first) && DebugGetContextMenuPopupItemRect(popup, 3u, third), "hover rows have rectangles");
            const double scale  = static_cast<double>(state.dpi) / 96.0;
            const LONG x        = static_cast<LONG>((first.left + first.right) * 0.5 * scale);
            const LONG firstY   = static_cast<LONG>((first.top + first.bottom) * 0.5 * scale);
            const LONG thirdY   = static_cast<LONG>((third.top + third.bottom) * 0.5 * scale);
            drive("hover", 32, [&](int index) { static_cast<void>(SendClientMouseMoveForMenuSuite(popup, x, index % 2 == 0 ? firstY : thirdY)); });
            Require(DebugGetContextMenuPopupState(popup, state) && state.hoveredIndex == std::optional<size_t>{3u}, "pointer moves hover the rows they reach");

            MenuUiaCostSeries closing;
            {
                const MenuUiaCostClock clock;
                close(popup, closed);
                clock.Stop(closing.wallUs, closing.kiloCycles);
            }
            WriteMenuUiaCostSeries(variant.name, round, "close", closing);
        }
    }
    std::cout << "{\"fixture\":\"dxui-menu-uia-cost-v1\",\"part\":\"environment\",\"uiaClientsListeningAtEnd\":" << (UiaClientsAreListening() ? "true" : "false")
              << "}\n";
}
