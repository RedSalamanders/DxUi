#include "Controls.Tests.MenuPopupReadiness.h"

#include "Controls.Tests.DxUiTestHelpers.h"

#include <array>

void TestMenuPopupDiscoveryRejectsHiddenInitializationWindows()
{
    // Never switch the input desktop or take the person's foreground, focus or pointer.
    const std::wstring desktopName = std::format(L"DxUi.PopupReadiness.{}", GetCurrentProcessId());
    const wil::unique_hdesk desktop(CreateDesktopW(desktopName.c_str(), nullptr, nullptr, 0, GENERIC_ALL, nullptr));
    Require(desktop != nullptr, "create a private popup-readiness desktop");
    std::thread worker([&]
    {
        Require(SetThreadDesktop(desktop.get()) != FALSE, "attach the readiness fixture to its non-input desktop");
        const auto makeWindow = [](HWND owner)
        {
            return wil::unique_hwnd(
                CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1, owner, nullptr, GetModuleHandleW(nullptr), nullptr));
        };
        const auto owner      = makeWindow(nullptr);
        const auto otherOwner = makeWindow(nullptr);
        const auto popup      = makeWindow(owner.get());
        const auto unrelated  = makeWindow(otherOwner.get());
        Require(owner && otherOwner && popup && unrelated, "create owned measurement windows");
        ShowWindow(owner.get(), SW_SHOWNOACTIVATE);
        ShowWindow(otherOwner.get(), SW_SHOWNOACTIVATE);
        ShowWindow(unrelated.get(), SW_SHOWNOACTIVATE);

        // No timing assumption: while initialization is held at the hidden stage, even a
        // matching class and owner cannot satisfy discovery. The old helper returned popup here.
        Require(! FindOwnedContextMenuPopupWindow(owner.get(), L"STATIC"), "ignore the hidden measurement HWND and another owner's visible popup");
        Require(! WaitForOwnedContextMenuPopupWindow(owner.get(), std::chrono::milliseconds(0), L"STATIC"),
                "a never-ready popup does not become a successful discovery");
        Require(! FindOwnedContextMenuPopupWindow(nullptr, L"STATIC"), "an absent owner cannot match an unowned window");

        ShowWindow(popup.get(), SW_SHOWNOACTIVATE);
        Require(IsWindowVisible(popup.get()) != FALSE, "release the synthetic initialization stage");
        Require(WaitForOwnedContextMenuPopupWindow(owner.get(), std::chrono::milliseconds(0), L"STATIC") == popup.get(),
                "the same HWND becomes discoverable only after initialization reveals it");
        Require(! FindOwnedContextMenuPopupWindow(owner.get(), L"BUTTON"), "discovery preserves its window-class filter");

        ShowWindow(popup.get(), SW_HIDE);
        Require(! FindOwnedContextMenuPopupWindow(owner.get(), L"STATIC"), "a hidden popup is no longer ready");
    });
    worker.join();
}

void TestPhysicalCursorReadbackWaitIsBoundedAndExact()
{
    using Clock = std::chrono::steady_clock;
    using namespace DxUi::Tests;

    const POINT target{244, 229};
    const POINT previous{244, 199};
    {
        PhysicalCursorOwnership ownership;
        ownership.RecordAcceptedTarget(target);
        Require(! ownership.lastConfirmedKnown && ownership.pendingTargetKnown && ownership.Contains(target),
                "an accepted first target is pending and is not mislabeled as confirmed");
        Require(! ownership.Contains(previous), "the stale readback does not become a confirmed target");

        ownership.RecordExactAlignment(previous);
        ownership.RecordAcceptedTarget(target);
        Require(ownership.lastConfirmedKnown && ownership.lastConfirmed.x == previous.x && ownership.lastConfirmed.y == previous.y,
                "an accepted new move preserves the last exact alignment");
        Require(ownership.Contains(previous) && ownership.Contains(target), "restoration recognizes the confirmed point and pending target");

        ownership.RecordExactAlignment(target);
        Require(ownership.lastConfirmedKnown && ownership.lastConfirmed.x == target.x && ownership.lastConfirmed.y == target.y &&
                    ! ownership.pendingTargetKnown,
                "exact convergence replaces the confirmed point and clears the pending target");
        Require(ownership.Contains(target) && ! ownership.Contains(previous), "a completed move no longer owns the stale point");
    }

    {
        Clock::duration elapsed{};
        const std::array<POINT, 4> readbacks = {previous, previous, target, target};
        std::size_t setCalls                 = 0u;
        std::size_t readCalls                = 0u;
        std::size_t sleepCalls               = 0u;
        const auto result                    = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                                        [&](POINT requested) noexcept
        {
            ++setCalls;
            return requested.x == target.x && requested.y == target.y;
        },
                                                                     [&](POINT& actual) noexcept
        {
            if (readCalls >= readbacks.size())
                return false;
            actual = readbacks[readCalls++];
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration;
        });

        Require(result.status == PhysicalCursorAlignmentStatus::Exact, "a stale readback converges to the exact requested point");
        Require(result.initialReadbackKnown && result.initialReadback.x == previous.x && result.initialReadback.y == previous.y,
                "the immediate stale position remains in the initial observation");
        Require(result.finalReadbackKnown && result.finalReadback.x == target.x && result.finalReadback.y == target.y,
                "the final observed position is the exact target");
        Require(result.pollCount == 3u && sleepCalls == 3u && result.elapsed == std::chrono::milliseconds(15),
                "the first exact read is provisional until the next 5 ms sample");
        Require(result.mismatchObserved, "the delayed convergence records the initial stale observation");
        Require(setCalls == 1u && readCalls == 4u, "convergence does not issue another cursor warp");
    }

    {
        Clock::duration elapsed{};
        std::size_t setCalls   = 0u;
        std::size_t readCalls  = 0u;
        std::size_t sleepCalls = 0u;
        const auto result      = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                          [&](POINT requested) noexcept
        {
            ++setCalls;
            return requested.x == target.x && requested.y == target.y;
        },
                                                                     [&](POINT& actual) noexcept
        {
            ++readCalls;
            actual = target;
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration;
        });

        Require(result.status == PhysicalCursorAlignmentStatus::Exact, "two settled exact observations succeed");
        Require(result.initialReadbackKnown && result.finalReadbackKnown && result.pollCount == 1u, "settled success records two exact observations");
        Require(setCalls == 1u && readCalls == 2u && sleepCalls == 1u && result.elapsed == std::chrono::milliseconds(5),
                "settled success confirms after one 5 ms read-only poll and one cursor set");
        Require(! result.mismatchObserved, "ordinary settled convergence does not record a mismatch");
    }

    {
        Clock::duration elapsed{};
        const std::array<POINT, 4> readbacks = {target, previous, target, target};
        std::size_t readCalls                = 0u;
        std::size_t sleepCalls               = 0u;
        const auto result                    = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                                        [&](POINT) noexcept { return true; },
                                                                                        [&](POINT& actual) noexcept
        {
            actual = readbacks[readCalls++];
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration;
        });

        Require(result.status == PhysicalCursorAlignmentStatus::Exact, "a mismatch resets the exact-observation streak before recovery");
        Require(result.initialReadback.x == target.x && result.initialReadback.y == target.y && result.mismatchObserved,
                "the first exact observation remains recorded when the next read mismatches");
        Require(result.finalReadbackKnown && result.finalReadback.x == target.x && result.finalReadback.y == target.y,
                "recovery completes only after two exact observations in sequence");
        Require(result.pollCount == 3u && readCalls == 4u && sleepCalls == 3u && result.elapsed == std::chrono::milliseconds(15),
                "the restarted exact streak uses the same 5 ms poll cadence");
    }

    {
        Clock::duration elapsed{};
        const POINT onePixelAway{target.x, target.y - 1};
        std::size_t setCalls   = 0u;
        std::size_t readCalls  = 0u;
        std::size_t sleepCalls = 0u;
        const auto result      = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                          [&](POINT) noexcept
        {
            ++setCalls;
            return true;
        },
                                                                     [&](POINT& actual) noexcept
        {
            ++readCalls;
            actual = onePixelAway;
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration;
        });

        Require(result.status == PhysicalCursorAlignmentStatus::TimedOut, "a nearby but inexact position times out");
        Require(result.pollCount == 19u && sleepCalls == 20u && result.elapsed == std::chrono::milliseconds(100),
                "the final sleep reaches the deadline without starting another read poll");
        Require(result.finalReadbackKnown && result.finalReadback.x == onePixelAway.x && result.finalReadback.y == onePixelAway.y,
                "the timeout retains its last exact physical observation");
        Require(setCalls == 1u && readCalls == 20u, "timeout does not repeat SetCursorPos");
    }

    {
        Clock::duration elapsed{};
        std::size_t setCalls   = 0u;
        std::size_t readCalls  = 0u;
        std::size_t sleepCalls = 0u;
        const auto result      = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                          [&](POINT) noexcept
        {
            ++setCalls;
            elapsed = std::chrono::milliseconds(100);
            return true;
        },
                                                                     [&](POINT& actual) noexcept
        {
            ++readCalls;
            actual = target;
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration;
        });

        Require(result.status == PhysicalCursorAlignmentStatus::TimedOut, "a cursor set completing at the deadline is refused");
        Require(setCalls == 1u && readCalls == 0u && sleepCalls == 0u && result.pollCount == 0u, "a late cursor set is not followed by a read or poll");
    }

    {
        Clock::duration elapsed{};
        std::size_t setCalls   = 0u;
        std::size_t readCalls  = 0u;
        std::size_t sleepCalls = 0u;
        const auto result      = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                          [&](POINT) noexcept
        {
            ++setCalls;
            return true;
        },
                                                                     [&](POINT&) noexcept
        {
            ++readCalls;
            return false;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration;
        });

        Require(result.status == PhysicalCursorAlignmentStatus::InitialReadFailed, "an immediate physical-read failure remains a refusal");
        Require(! result.initialReadbackKnown && result.pollCount == 0u && sleepCalls == 0u, "an immediate read failure does not enter the convergence wait");
        Require(setCalls == 1u && readCalls == 1u, "an immediate read failure does not repeat the cursor warp");
    }

    {
        Clock::duration elapsed{};
        std::size_t setCalls   = 0u;
        std::size_t readCalls  = 0u;
        std::size_t sleepCalls = 0u;
        const auto result      = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                          [&](POINT) noexcept
        {
            ++setCalls;
            return true;
        },
                                                                     [&](POINT& actual) noexcept
        {
            if (readCalls++ == 0u)
            {
                actual = previous;
                return true;
            }
            return false;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration;
        });

        Require(result.status == PhysicalCursorAlignmentStatus::PollReadFailed, "a later physical-read failure remains a refusal");
        Require(result.initialReadbackKnown && result.pollCount == 1u && sleepCalls == 1u,
                "a polling read failure retains the initial stale position and bounded attempt count");
        Require(! result.finalReadbackKnown, "a failed final read is not reported as a known final position");
        Require(setCalls == 1u && readCalls == 2u, "a polling read failure does not repeat the cursor warp");
    }

    {
        Clock::duration elapsed{};
        std::size_t setCalls  = 0u;
        std::size_t readCalls = 0u;
        const auto result     = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                         [&](POINT) noexcept
        {
            ++setCalls;
            return false;
        },
                                                                     [&](POINT&) noexcept
        {
            ++readCalls;
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept { elapsed += duration; });

        Require(result.status == PhysicalCursorAlignmentStatus::SetFailed, "a failed cursor set remains a refusal");
        Require(setCalls == 1u && readCalls == 0u && result.pollCount == 0u, "a failed cursor set is not followed by reads or another warp");
    }

    {
        Clock::duration elapsed{};
        std::size_t setCalls   = 0u;
        std::size_t readCalls  = 0u;
        std::size_t sleepCalls = 0u;
        const auto result      = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                          [&](POINT) noexcept
        {
            ++setCalls;
            return true;
        },
                                                                     [&](POINT& actual) noexcept
        {
            ++readCalls;
            actual  = target;
            elapsed = std::chrono::milliseconds(100);
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration;
        });

        Require(result.status == PhysicalCursorAlignmentStatus::TimedOut, "an exact initial read completing at the deadline is refused");
        Require(result.initialReadbackKnown && result.finalReadbackKnown && result.finalReadback.x == target.x && result.finalReadback.y == target.y,
                "a late exact initial observation remains recorded without qualifying as timely");
        Require(setCalls == 1u && readCalls == 1u && sleepCalls == 0u && result.pollCount == 0u, "a late initial read does not begin polling");
    }

    {
        Clock::duration elapsed{};
        std::size_t setCalls   = 0u;
        std::size_t readCalls  = 0u;
        std::size_t sleepCalls = 0u;
        const auto result      = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                          [&](POINT) noexcept
        {
            ++setCalls;
            return true;
        },
                                                                     [&](POINT& actual) noexcept
        {
            ++readCalls;
            if (readCalls == 1u)
            {
                actual = previous;
            }
            else
            {
                actual  = target;
                elapsed = std::chrono::milliseconds(100);
            }
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration;
        });

        Require(result.status == PhysicalCursorAlignmentStatus::TimedOut, "an exact polling read completing at the deadline is refused");
        Require(result.initialReadbackKnown && result.finalReadbackKnown && result.finalReadback.x == target.x && result.finalReadback.y == target.y,
                "the late exact polling observation is retained but not qualified");
        Require(setCalls == 1u && readCalls == 2u && sleepCalls == 1u && result.pollCount == 1u, "a late polling read does not trigger another poll");
    }

    {
        Clock::duration elapsed{};
        std::size_t setCalls   = 0u;
        std::size_t readCalls  = 0u;
        std::size_t sleepCalls = 0u;
        const auto result      = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                          [&](POINT) noexcept
        {
            ++setCalls;
            return true;
        },
                                                                     [&](POINT& actual) noexcept
        {
            ++readCalls;
            if (readCalls == 1u)
            {
                actual = previous;
            }
            else
            {
                actual = target;
                if (readCalls == 3u)
                {
                    elapsed = std::chrono::milliseconds(100);
                }
            }
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration;
        });

        Require(result.status == PhysicalCursorAlignmentStatus::TimedOut, "a second exact observation completing at the deadline cannot establish readiness");
        Require(result.initialReadbackKnown && result.finalReadbackKnown && result.finalReadback.x == target.x && result.finalReadback.y == target.y,
                "the late confirming sample remains diagnostic without qualifying");
        Require(setCalls == 1u && readCalls == 3u && sleepCalls == 2u && result.pollCount == 2u,
                "late confirmation remains inside the original poll budget without another warp");
    }

    {
        Clock::duration elapsed{};
        std::size_t setCalls   = 0u;
        std::size_t readCalls  = 0u;
        std::size_t sleepCalls = 0u;
        const auto result      = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                          [&](POINT) noexcept
        {
            ++setCalls;
            return true;
        },
                                                                     [&](POINT& actual) noexcept
        {
            ++readCalls;
            actual = previous;
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration duration) noexcept
        {
            ++sleepCalls;
            elapsed += duration + std::chrono::milliseconds(100);
        });

        Require(result.status == PhysicalCursorAlignmentStatus::TimedOut && result.elapsed == std::chrono::milliseconds(105),
                "scheduler overshoot is reported without claiming a hard wall-clock bound");
        Require(result.pollCount == 0u && readCalls == 1u && sleepCalls == 1u, "oversleep prevents a poll from starting at or after the observed deadline");
        Require(setCalls == 1u, "oversleep never repeats the cursor warp");
    }

    {
        Clock::duration elapsed{};
        std::size_t setCalls   = 0u;
        std::size_t readCalls  = 0u;
        std::size_t sleepCalls = 0u;
        const auto result      = SetCursorPosAndWaitForExactPhysicalPoint(target,
                                                                          [&](POINT) noexcept
        {
            ++setCalls;
            return true;
        },
                                                                     [&](POINT& actual) noexcept
        {
            ++readCalls;
            actual = previous;
            return true;
        },
                                                                     [&]() noexcept { return Clock::time_point{} + elapsed; },
                                                                     [&](Clock::duration) noexcept { ++sleepCalls; });

        Require(result.status == PhysicalCursorAlignmentStatus::TimedOut && result.elapsed == Clock::duration::zero(),
                "the fixed poll budget terminates even if the injected clock does not advance");
        Require(result.pollCount == 20u && readCalls == 21u && sleepCalls == 20u && setCalls == 1u,
                "a stationary clock cannot exceed the twenty-poll budget or repeat the warp");
    }
}
