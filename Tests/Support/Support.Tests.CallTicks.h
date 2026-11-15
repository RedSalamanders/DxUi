#pragma once
// Time spent inside calls the counting COM wrappers forward (Support.Tests.CountingDirect2D.h, Support.Tests.CountingWriteFactory.h),
// for the hardware attribution method only (plan W0.7; Specs/Core/Core_PerformanceAndResources.md, "Hardware attribution
// method"). Dormant unless a test installs a CallTicks record on its thread: a wrapped call then adds the time it spent inside
// the real object to one bucket. Nothing here is library code; the library's paths carry no timer.
//
// Units: the processor time-stamp counter on x64 (the clock QueryThreadCycleTime counts in, so a bucket compares directly with
// a thread's cycles), QueryPerformanceCounter elsewhere. The interval is wall time inside the call: it includes any preemption
// during the call, which a sub-millisecond call on a pinned thread rarely sees.
#include <cstdint>
#include <intrin.h>
#include <windows.h>

namespace NativeWorkTests
{
struct CallTicks final
{
    uint64_t beginDraw   = 0u; // ID2D1RenderTarget::BeginDraw
    uint64_t endDraw     = 0u; // ID2D1RenderTarget::EndDraw: Direct2D's batched submission to Direct3D 11 and the driver
    uint64_t flush       = 0u; // ID2D1RenderTarget::Flush
    uint64_t drawCalls   = 0u; // every counted Direct2D draw call (Clear, Fill..., Draw..., DrawTextLayout, DrawImage...)
    uint64_t textLayouts = 0u; // IDWriteFactory::CreateTextLayout and CreateGdiCompatibleTextLayout
    uint64_t textFormats = 0u; // IDWriteFactory::CreateTextFormat
};

// The calling thread's record, or nullptr (the default): nothing is timed.
inline thread_local constinit CallTicks* g_callTicks = nullptr;

[[nodiscard]] inline uint64_t ReadCallTicks() noexcept
{
#if defined(_M_X64)
    return __rdtsc();
#else
    LARGE_INTEGER value{};
    static_cast<void>(QueryPerformanceCounter(&value));
    return static_cast<uint64_t>(value.QuadPart);
#endif
}

// Adds the time from construction to destruction to one bucket of the thread's record, when one is installed.
template <uint64_t CallTicks::* Bucket> class TimedCall final
{
public:
    TimedCall() noexcept : _ticks(g_callTicks), _start(_ticks ? ReadCallTicks() : 0u)
    {
    }
    ~TimedCall()
    {
        if (_ticks)
            _ticks->*Bucket += ReadCallTicks() - _start;
    }
    TimedCall(const TimedCall&)            = delete;
    TimedCall& operator=(const TimedCall&) = delete;
    TimedCall(TimedCall&&)                 = delete;
    TimedCall& operator=(TimedCall&&)      = delete;

private:
    CallTicks* _ticks = nullptr;
    uint64_t _start   = 0u;
};
} // namespace NativeWorkTests
