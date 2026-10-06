#pragma once

#include <Windows.h>

#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027 28182)
#include <wil/resource.h>
#pragma warning(pop)

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <thread>

namespace DxUi::TestSupport
{
// Test-only stand-in for a desktop application that takes the foreground back shortly after one of this process's
// windows activated: the application hosting a developer's session did so 30-95 ms after each test window. A worker
// thread owns a tiny window and, a random delay in [minDelayMs, maxDelayMs] after a window of this process became the
// foreground window, makes its own window the foreground window. The activated window then receives what a foreign
// application's window would send it (WM_NCACTIVATE, WM_ACTIVATE, WM_ACTIVATEAPP, WM_KILLFOCUS), when its thread next
// dispatches messages. Opt in with --foreground-thief; it never runs by itself.
class ForegroundThief final
{
public:
    ForegroundThief(DWORD minDelayMs, DWORD maxDelayMs) : _minDelayMs(minDelayMs), _maxDelayMs(maxDelayMs), _thread([this](std::stop_token stop) { Run(stop); })
    {
    }

    ForegroundThief(const ForegroundThief&)            = delete;
    ForegroundThief& operator=(const ForegroundThief&) = delete;
    ForegroundThief(ForegroundThief&&)                 = delete;
    ForegroundThief& operator=(ForegroundThief&&)      = delete;

    // How often the thief took the foreground so far.
    [[nodiscard]] uint64_t TheftCount() const noexcept
    {
        return _theftCount.load(std::memory_order_acquire);
    }

    // Whether a window of this process ever held the foreground. When Windows keeps it with the application the user is
    // working in, no test window gets it, there is nothing to take and a run exercised no takeover.
    [[nodiscard]] bool OwnForegroundSeen() const noexcept
    {
        return _ownForegroundSeen.load(std::memory_order_acquire);
    }

private:
    static constexpr PCWSTR kWindowClassName = L"DxUiTests.ForegroundThief";

    void Run(const std::stop_token& stop) noexcept
    {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc   = &DefWindowProcW;
        windowClass.hInstance     = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = kWindowClassName;
        static_cast<void>(RegisterClassW(&windowClass));
        const wil::unique_hwnd thief(CreateWindowExW(
            WS_EX_TOOLWINDOW, kWindowClassName, L"DxUiTestsForegroundThief", WS_POPUP, -32000, -32000, 1, 1, nullptr, nullptr, windowClass.hInstance, nullptr));
        if (! thief)
            return;
        static_cast<void>(ShowWindow(thief.get(), SW_SHOWNOACTIVATE));

        std::mt19937 random(GetTickCount());
        std::uniform_int_distribution<DWORD> delayMs(_minDelayMs, _maxDelayMs);
        const DWORD thiefThreadId = GetCurrentThreadId();
        HWND watched              = nullptr; // The window of this process whose activation the thief is timing.
        std::chrono::steady_clock::time_point due{};
        while (! stop.stop_requested())
        {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE)
                DispatchMessageW(&msg);

            DWORD foregroundProcessId      = 0;
            const HWND foreground          = GetForegroundWindow();
            const DWORD foregroundThreadId = foreground ? GetWindowThreadProcessId(foreground, &foregroundProcessId) : 0u;
            if (foreground && foregroundProcessId == GetCurrentProcessId() && foregroundThreadId != thiefThreadId)
            {
                _ownForegroundSeen.store(true, std::memory_order_release);
                if (watched != foreground)
                {
                    watched = foreground;
                    due     = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs(random));
                }
                else if (std::chrono::steady_clock::now() >= due)
                {
                    if (SetForegroundWindow(thief.get()) != FALSE)
                        _theftCount.fetch_add(1u, std::memory_order_acq_rel);
                    watched = nullptr;
                }
            }
            else
            {
                watched = nullptr;
            }
            static_cast<void>(MsgWaitForMultipleObjects(0, nullptr, FALSE, 2, QS_ALLINPUT));
        }
    }

    DWORD _minDelayMs;
    DWORD _maxDelayMs;
    std::atomic<uint64_t> _theftCount{0u};
    std::atomic<bool> _ownForegroundSeen{false};
    std::jthread _thread; // Declared last: it starts in the constructor and joins first when the members are destroyed.
};
} // namespace DxUi::TestSupport
