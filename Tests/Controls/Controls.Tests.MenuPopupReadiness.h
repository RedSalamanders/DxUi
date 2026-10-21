#pragma once

#include <windows.h>

#include <chrono>
#include <string>
#include <thread>

// A popup's measurement HWND exists before resource preparation, layout and its initial frame.
// Discovery must wait for the visible window, without probing that hidden initialization stage.
[[nodiscard]] inline bool IsReadyOwnedContextMenuPopupWindow(HWND popupHwnd, HWND ownerHwnd) noexcept
{
    if (! popupHwnd || ! ownerHwnd)
        return false;

    DWORD processId = 0u;
    static_cast<void>(GetWindowThreadProcessId(popupHwnd, &processId));
    return processId == GetCurrentProcessId() && GetWindow(popupHwnd, GW_OWNER) == ownerHwnd && IsWindowVisible(popupHwnd) != FALSE;
}

// The class argument lets the regression use ordinary windows on a private, non-input desktop.
[[nodiscard]] inline HWND FindOwnedContextMenuPopupWindow(HWND ownerHwnd, const wchar_t* windowClass = L"DxUi_ContextMenu") noexcept
{
    if (! ownerHwnd)
        return nullptr;

    HWND popupHwnd = nullptr;
    while ((popupHwnd = FindWindowExW(nullptr, popupHwnd, windowClass, nullptr)) != nullptr)
    {
        if (IsReadyOwnedContextMenuPopupWindow(popupHwnd, ownerHwnd))
            return popupHwnd;
    }
    return nullptr;
}

[[nodiscard]] inline HWND WaitForOwnedContextMenuPopupWindow(HWND ownerHwnd,
                                                             std::chrono::milliseconds timeout = std::chrono::milliseconds(5000),
                                                             const wchar_t* windowClass        = L"DxUi_ContextMenu")
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do
    {
        if (HWND popup = FindOwnedContextMenuPopupWindow(ownerHwnd, windowClass))
            return popup;
        if (std::chrono::steady_clock::now() >= deadline)
            return nullptr;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    return nullptr;
}

void TestMenuPopupDiscoveryRejectsHiddenInitializationWindows();

inline bool WaitForWindowDestroyed(HWND hwnd, std::chrono::milliseconds timeout = std::chrono::milliseconds(800))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do
    {
        if (IsWindow(hwnd) == FALSE)
        {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);

    return IsWindow(hwnd) == FALSE;
}

// Closes the popup the owner has open, if it has one; returns whether there was one.
inline bool DismissOwnedContextMenuPopupOnce(HWND ownerHwnd) noexcept
{
    const HWND popupHwnd = FindOwnedContextMenuPopupWindow(ownerHwnd);
    if (! popupHwnd)
    {
        return false;
    }

    DWORD_PTR unused = 0;
    static_cast<void>(SendMessageTimeoutW(popupHwnd, WM_KEYDOWN, VK_ESCAPE, 0, SMTO_ABORTIFHUNG, 1000u, &unused));
    static_cast<void>(WaitForWindowDestroyed(popupHwnd, std::chrono::milliseconds(120)));
    return true;
}

inline void DismissOwnedContextMenuPopupChain(HWND ownerHwnd) noexcept
{
    for (int attempt = 0; attempt < 16; ++attempt)
    {
        if (! DismissOwnedContextMenuPopupOnce(ownerHwnd))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }
}

// Closes every popup the owner opens within `allowance`, and stops early once the popups it closed are gone for good. Returns
// whether it closed any.
inline bool DismissOwnedContextMenusForAtMost(HWND ownerHwnd, std::chrono::milliseconds allowance) noexcept
{
    const auto deadline = std::chrono::steady_clock::now() + allowance;
    bool dismissedAny   = false;
    int quietPolls      = 0;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (DismissOwnedContextMenuPopupOnce(ownerHwnd))
        {
            dismissedAny = true;
            quietPolls   = 0;
            continue;
        }

        if (dismissedAny && ++quietPolls >= 5)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    return dismissedAny;
}

// ContextMenu::Show runs its modal loop on the owner thread and returns only when the menu closes, and the driver thread is what
// closes it. A driver that fails before it has a popup to dismiss (the popup can come up later than the driver's wait on a slow
// runner) would leave the owner thread in that loop for good: the test would hang where it should report the failure the driver
// recorded. Declare it first in a driver, before its first wait: when the driver leaves with a failure it keeps closing what
// comes up, for a few seconds. It does nothing when the driver succeeded, which has closed the menu itself. A test that wants to
// know whether it closed a menu passes `dismissedAny`, which is written when the driver leaves.
class DismissMenusIfDriverFails final
{
public:
    DismissMenusIfDriverFails(HWND ownerHwnd, const std::string& driverFailure, bool* dismissedAny = nullptr) noexcept
        : _ownerHwnd(ownerHwnd),
          _driverFailure(driverFailure),
          _dismissedAny(dismissedAny)
    {
    }
    DismissMenusIfDriverFails(const DismissMenusIfDriverFails&)            = delete;
    DismissMenusIfDriverFails& operator=(const DismissMenusIfDriverFails&) = delete;
    DismissMenusIfDriverFails(DismissMenusIfDriverFails&&)                 = delete;
    DismissMenusIfDriverFails& operator=(DismissMenusIfDriverFails&&)      = delete;

    ~DismissMenusIfDriverFails()
    {
        if (_driverFailure.empty())
        {
            return;
        }

        const bool dismissed = DismissOwnedContextMenusForAtMost(_ownerHwnd, std::chrono::seconds(8));
        if (_dismissedAny)
        {
            *_dismissedAny = dismissed;
        }
    }

private:
    HWND _ownerHwnd;
    const std::string& _driverFailure;
    bool* _dismissedAny;
};
