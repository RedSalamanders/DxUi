#pragma once
// Application clocks for embedded test views (API revision 6 requires EmbeddedCallbacks::nowTickMs).
//
// StoppedTestClock: a clock that reads 0, so between advances the view's timeline never moves: every stamp taken between
// advances takes the last advance's time, exactly as before the clock existed. Tests that drive time only through
// AdvanceAnimation (with any epoch) keep their meaning with it. A test about stamps taken between advances uses a running
// clock instead (RunningTestClock, or the performance fixtures' clock).
#include <DxUi/Embedded.h>

#include <cstdint>

[[nodiscard]] inline uint64_t StoppedTestClockMs(void*) noexcept
{
    return 0u;
}

// The given callbacks with the stopped clock added.
[[nodiscard]] inline DxUi::EmbeddedCallbacks StoppedTestClock(DxUi::EmbeddedCallbacks callbacks = {}) noexcept
{
    callbacks.nowTickMs = &StoppedTestClockMs;
    return callbacks;
}

// A clock a test sets: the view reads `nowMs` whenever it takes a stamp between advances.
struct RunningTestClock
{
    uint64_t nowMs                                     = 0u;
    void (*requestPreparation)(void* context) noexcept = nullptr;
    void* requestContext                               = nullptr;
    size_t wakes                                       = 0u;
    static uint64_t Now(void* context) noexcept
    {
        return static_cast<RunningTestClock*>(context)->nowMs;
    }
    static void Wake(void* context) noexcept
    {
        auto* const clock = static_cast<RunningTestClock*>(context);
        ++clock->wakes;
        if (clock->requestPreparation)
            clock->requestPreparation(clock->requestContext);
    }
    [[nodiscard]] DxUi::EmbeddedCallbacks Callbacks() noexcept
    {
        return DxUi::EmbeddedCallbacks{this, &Wake, &Now};
    }
};
