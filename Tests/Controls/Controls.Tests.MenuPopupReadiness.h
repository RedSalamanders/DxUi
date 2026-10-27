#pragma once

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace DxUi::Tests
{

enum class PhysicalCursorAlignmentStatus : std::uint8_t
{
    Exact,
    SetFailed,
    InitialReadFailed,
    PollReadFailed,
    TimedOut,
};

struct PhysicalCursorAlignmentResult
{
    PhysicalCursorAlignmentStatus status = PhysicalCursorAlignmentStatus::TimedOut;
    POINT initialReadback{};
    POINT finalReadback{};
    bool initialReadbackKnown = false;
    bool finalReadbackKnown   = false;
    std::uint32_t pollCount   = 0u;
    std::chrono::microseconds elapsed{};
};

struct PhysicalCursorOwnership final
{
    POINT lastConfirmed{};
    POINT pendingTarget{};
    bool lastConfirmedKnown = false;
    bool pendingTargetKnown = false;

    void RecordAcceptedTarget(POINT target) noexcept
    {
        pendingTarget      = target;
        pendingTargetKnown = true;
    }

    void RecordExactAlignment(POINT target) noexcept
    {
        lastConfirmed      = target;
        lastConfirmedKnown = true;
        pendingTargetKnown = false;
    }

    [[nodiscard]] bool HasOwnedPoint() const noexcept
    {
        return lastConfirmedKnown || pendingTargetKnown;
    }

    [[nodiscard]] bool Contains(POINT point) const noexcept
    {
        const bool atLastConfirmed = lastConfirmedKnown && point.x == lastConfirmed.x && point.y == lastConfirmed.y;
        const bool atPendingTarget = pendingTargetKnown && point.x == pendingTarget.x && point.y == pendingTarget.y;
        return atLastConfirmed || atPendingTarget;
    }
};

[[nodiscard]] inline const char* PhysicalCursorAlignmentStatusText(PhysicalCursorAlignmentStatus status) noexcept
{
    switch (status)
    {
        case PhysicalCursorAlignmentStatus::Exact: return "exact";
        case PhysicalCursorAlignmentStatus::SetFailed: return "set-failed";
        case PhysicalCursorAlignmentStatus::InitialReadFailed: return "initial-read-failed";
        case PhysicalCursorAlignmentStatus::PollReadFailed: return "poll-read-failed";
        case PhysicalCursorAlignmentStatus::TimedOut: return "timed-out";
    }

    return "unknown";
}

// The hosted desktop can expose a stale immediate physical-cursor read after
// SetCursorPos accepts a move. Poll read-only for the one requested point while
// the observed deadline and poll budget allow; synchronous calls or scheduling
// can return after the deadline. Never issue another cursor move or accept a nearby point.
template <typename TSetCursorPos, typename TReadPhysicalCursor, typename TNow, typename TSleep>
[[nodiscard]] inline PhysicalCursorAlignmentResult SetCursorPosAndWaitForExactPhysicalPoint(
    POINT expected, TSetCursorPos&& setCursorPos, TReadPhysicalCursor&& readPhysicalCursor, TNow&& now, TSleep&& sleep)
{
    using Clock = std::chrono::steady_clock;

    constexpr auto timeout               = std::chrono::milliseconds{100};
    constexpr auto pollInterval          = std::chrono::milliseconds{5};
    constexpr std::uint32_t maximumPolls = 20u;

    PhysicalCursorAlignmentResult result;
    const auto startedAt = now();
    const auto deadline  = startedAt + timeout;
    const auto finish    = [&](PhysicalCursorAlignmentStatus status)
    {
        result.status  = status;
        result.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now() - startedAt);
        return result;
    };
    const auto isExact = [expected](POINT point) noexcept { return point.x == expected.x && point.y == expected.y; };

    if (! setCursorPos(expected))
    {
        return finish(PhysicalCursorAlignmentStatus::SetFailed);
    }

    if (now() >= deadline)
    {
        return finish(PhysicalCursorAlignmentStatus::TimedOut);
    }

    POINT observed{};
    if (! readPhysicalCursor(observed))
    {
        return finish(PhysicalCursorAlignmentStatus::InitialReadFailed);
    }

    result.initialReadback      = observed;
    result.initialReadbackKnown = true;
    result.finalReadback        = observed;
    result.finalReadbackKnown   = true;
    if (now() >= deadline)
    {
        return finish(PhysicalCursorAlignmentStatus::TimedOut);
    }

    if (isExact(observed))
    {
        return finish(PhysicalCursorAlignmentStatus::Exact);
    }

    while (result.pollCount < maximumPolls && now() < deadline)
    {
        const auto loopTime = now();
        if (loopTime >= deadline)
        {
            break;
        }

        const auto remaining = deadline - loopTime;
        const auto interval  = std::chrono::duration_cast<Clock::duration>(pollInterval);
        sleep(remaining < interval ? remaining : interval);
        if (now() >= deadline)
        {
            break;
        }

        ++result.pollCount;
        if (! readPhysicalCursor(observed))
        {
            result.finalReadbackKnown = false;
            return finish(PhysicalCursorAlignmentStatus::PollReadFailed);
        }

        result.finalReadback      = observed;
        result.finalReadbackKnown = true;
        if (now() >= deadline)
        {
            return finish(PhysicalCursorAlignmentStatus::TimedOut);
        }

        if (isExact(observed))
        {
            return finish(PhysicalCursorAlignmentStatus::Exact);
        }
    }

    return finish(PhysicalCursorAlignmentStatus::TimedOut);
}

} // namespace DxUi::Tests

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
void TestPhysicalCursorReadbackWaitIsBoundedAndExact();

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
