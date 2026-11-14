#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#pragma warning(push)
// Windows headers: C4710 (not inlined), C4711 (auto inline), C4514 (unreferenced inline)
#pragma warning(disable : 4710 4711 4514)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#pragma warning(pop)

#include "Diagnostics.h"
#include "Trace.h"
#include "WorkCounters.h"
#include <DxUi/Failure.h>
#include <DxUi/FrameRuntime.h>

#pragma warning(push)
// WIL: C4625 (copy ctor deleted), C4626 (copy assign deleted), C5026 (move ctor deleted), C5027 (move assign deleted)
#pragma warning(disable : 4625 4626 5026 5027 28182)
#include <wil/resource.h>
#include <wil/win32_helpers.h>
#pragma warning(pop)

namespace DxUi::Ui
{
// The UI thread's native wake source (W3.1). Two kinds of subscriber share one message-only window:
//  - a tick subscriber (Subscribe) is ticked on every frame until its callback returns false;
//  - a schedule subscriber (SubscribeSchedule; one per attached native ControlHost) names the next wake it needs:
//    none, every frame (Continuous) or once its deadline is due (Deadline). Its callback runs at that wake and returns
//    the next one; Reschedule changes it between wakes.
// One periodic timer runs only while some subscriber needs every frame; otherwise one coalescable one-shot timer is armed
// for the earliest deadline across the thread's subscribers, and nothing is armed when nobody waits. The clock: while
// the periodic timer runs, the timeline advances by smoothed frame deltas from the real time of its first tick (no
// phantom first frame); otherwise it is GetTickCount64.
class AnimationDispatcher final
{
private:
    struct TickTiming
    {
        uint64_t rawDeltaUs      = 0u;
        uint64_t callbackDeltaUs = 0u;
        uint64_t legacyGapMs     = 0u;
        bool legacyOverrun       = false;
    };

public:
    enum class WakeKind : uint8_t
    {
        None,
        Continuous,
        Deadline,
    };
    struct Wake final
    {
        WakeKind kind      = WakeKind::None;
        uint64_t dueTickMs = 0u;
    };

    using TickCallback     = bool (*)(void* context, uint64_t nowTickMs) noexcept;
    using ScheduleCallback = Wake (*)(void* context, uint64_t nowTickMs) noexcept;

    AnimationDispatcher() noexcept = default;
    // Thread-local destruction (thread or process exit) may find a window that a host left behind. Drop every subscriber
    // without calling it and stop the timers first, so the window's teardown messages run no DxUi work (WndProcThunk).
    ~AnimationDispatcher()
    {
        _subscriptions.clear();
        _pendingAdds.clear();
        if (_hwnd)
        {
            KillTimer(_hwnd.get(), kTimerId);
            KillTimer(_hwnd.get(), kDeadlineTimerId);
            SetWindowLongPtrW(_hwnd.get(), GWLP_USERDATA, 0);
        }
    }

    AnimationDispatcher(const AnimationDispatcher&)            = delete;
    AnimationDispatcher(AnimationDispatcher&&)                 = delete;
    AnimationDispatcher& operator=(const AnimationDispatcher&) = delete;
    AnimationDispatcher& operator=(AnimationDispatcher&&)      = delete;

    [[nodiscard]] static AnimationDispatcher& GetInstance() noexcept
    {
        // Window-mode hosts are UI-thread scoped and detach before thread shutdown.
        static thread_local AnimationDispatcher instance;
        return instance;
    }

    [[nodiscard]] uint64_t GetCurrentTickMs() const noexcept
    {
        return _hasTickTimestamp ? _lastTickMs : GetTickCount64();
    }

    void RetainIdleWindow() noexcept
    {
        _retainIdleWindow = true;
    }

    void RetireIdleWindow() noexcept
    {
        _retainIdleWindow = false;
        if (! _inTick)
            EnsureTimerState();
    }

    void Shutdown() noexcept
    {
        StopTimer();
        StopDeadlineTimer();
        _subscriptions.clear();
        _pendingAdds.clear();
        // Identifiers keep counting: an owner still holding one from before finds nothing rather than a new subscriber.
        _inTick            = false;
        _timerRunning      = false;
        _lastTickMs        = 0u;
        _tickEpochMs       = 0u;
        _elapsedTickUs     = 0u;
        _lastTickTimestamp = {};
        _hasTickTimestamp  = false;
        _hwnd.reset();
    }

    // A tick subscriber: ticked on every frame until its callback returns false.
    [[nodiscard]] uint64_t Subscribe(TickCallback callback, void* context) noexcept
    {
        if (! callback)
            return 0;
        Subscription entry{};
        entry.tick = callback;
        entry.wake = Wake{WakeKind::Continuous, 0u};
        return Add(entry, context);
    }

    // A schedule subscriber: woken as its Wake asks, never removed by its callback. The owner unsubscribes it.
    [[nodiscard]] uint64_t SubscribeSchedule(ScheduleCallback callback, void* context, Wake wake) noexcept
    {
        if (! callback)
            return 0;
        Subscription entry{};
        entry.schedule = callback;
        entry.wake     = wake;
        return Add(entry, context);
    }

    // Changes what a schedule subscriber waits for. A change made during a wake takes effect when the wake ends.
    void Reschedule(uint64_t id, Wake wake) noexcept
    {
        if (id == 0u)
            return;
        Subscription* const entry = Find(id);
        if (! entry || (entry->wake.kind == wake.kind && entry->wake.dueTickMs == wake.dueTickMs))
            return;
        entry->wake = wake;
        if (! _inTick)
            EnsureTimerState();
    }

    [[nodiscard]] bool IsSubscribed(uint64_t id) noexcept
    {
        return id != 0u && Find(id) != nullptr;
    }

    void Unsubscribe(uint64_t id) noexcept
    {
        if (id == 0)
        {
            return;
        }

        MarkPendingRemove(_subscriptions, id);
        MarkPendingRemove(_pendingAdds, id);

        if (! _inTick)
        {
            GarbageCollect();
            EnsureTimerState();
        }
    }

#if DXUI_ENABLE_DIAGNOSTICS
    [[nodiscard]] size_t DebugGetActiveSubscriptionCountForTest() const noexcept
    {
        return CountActiveSubscriptions(_subscriptions) + CountActiveSubscriptions(_pendingAdds);
    }
    // Whether the periodic (every-frame) timer or the one-shot deadline timer is armed, and the deadline it is armed for.
    [[nodiscard]] bool DebugIsPeriodicTimerRunningForTest() const noexcept
    {
        return _timerRunning;
    }
    [[nodiscard]] bool DebugIsDeadlineTimerArmedForTest() const noexcept
    {
        return _deadlineArmed;
    }
    [[nodiscard]] uint64_t DebugGetArmedDeadlineTickMsForTest() const noexcept
    {
        return _armedDeadlineTickMs;
    }
    struct DebugTimingForTest
    {
        uint64_t callbackDeltaUs = 0u;
        uint64_t legacyGapMs     = 0u;
        bool legacyOverrun       = false;
    };

    [[nodiscard]] uint64_t DebugGetTargetFrameUsForTest() const noexcept
    {
        return _frameBudget.refreshPeriodUs;
    }

    [[nodiscard]] uint64_t DebugGetHitchClampUsForTest() const noexcept
    {
        return _frameBudget.hitchClampUs;
    }

    [[nodiscard]] DebugTimingForTest DebugComputeTimingForTest(uint64_t rawDeltaUs) noexcept
    {
        const TickTiming timing = ComputeTickTiming(rawDeltaUs);
        return DebugTimingForTest{
            timing.callbackDeltaUs,
            timing.legacyGapMs,
            timing.legacyOverrun,
        };
    }
#endif

private:
    static constexpr wchar_t kWindowClassName[] = L"DxUi.AnimationDispatcher";
    static constexpr UINT_PTR kTimerId          = 1;
    static constexpr UINT_PTR kDeadlineTimerId  = 2;
    static constexpr UINT kFrameIntervalMs      = 8u;
    // GetTickCount64 advances in steps of up to one system tick (15.6 ms), so a one-shot timer armed for exactly the
    // remaining time can wake while the clock still reads a little before the deadline. Arming one tick later makes
    // the wake see the deadline as due instead of costing a second wake.
    static constexpr uint64_t kDeadlineSlackMs = 16u;
    static constexpr uint64_t kTargetFrameUs   = 1'000'000u / 120u;
    static constexpr uint64_t kHitchClampUs    = 50'000u;

    struct Subscription
    {
        uint64_t id               = 0;
        TickCallback tick         = nullptr;
        ScheduleCallback schedule = nullptr;
        void* context             = nullptr;
        Wake wake{};
        bool pendingRemove = false;
    };

    [[nodiscard]] uint64_t Add(Subscription entry, void* context) noexcept
    {
        const Failure::Scope failureScope;
        EnsureWindow();
        if (! _hwnd)
        {
            return 0;
        }

        entry.id            = _nextSubscriptionId;
        entry.context       = context;
        entry.pendingRemove = false;
        _nextSubscriptionId += 1u;

        if (_inTick)
        {
            _pendingAdds.push_back(entry);
        }
        else
        {
            _subscriptions.push_back(entry);
            EnsureTimerState();
        }
        return entry.id;
    }

    [[nodiscard]] Subscription* Find(uint64_t id) noexcept
    {
        for (auto& entry : _subscriptions)
            if (entry.id == id && ! entry.pendingRemove)
                return &entry;
        for (auto& entry : _pendingAdds)
            if (entry.id == id && ! entry.pendingRemove)
                return &entry;
        return nullptr;
    }

    static void MarkPendingRemove(std::vector<Subscription>& list, uint64_t id) noexcept
    {
        for (auto& entry : list)
        {
            if (entry.id == id)
            {
                entry.pendingRemove = true;
                return;
            }
        }
    }

    [[nodiscard]] static bool IsActive(const Subscription& entry) noexcept
    {
        return ! entry.pendingRemove && (entry.tick != nullptr || entry.schedule != nullptr);
    }

    [[nodiscard]] static uint64_t AbsoluteDifference(uint64_t lhs, uint64_t rhs) noexcept
    {
        return lhs >= rhs ? lhs - rhs : rhs - lhs;
    }

    [[nodiscard]] static size_t CountActiveSubscriptions(const std::vector<Subscription>& list) noexcept
    {
        size_t count = 0u;
        for (const auto& entry : list)
        {
            if (IsActive(entry))
            {
                ++count;
            }
        }

        return count;
    }

    [[nodiscard]] uint64_t AddElapsedTickUs(uint64_t deltaUs) noexcept
    {
        if (deltaUs > std::numeric_limits<uint64_t>::max() - _elapsedTickUs)
        {
            _elapsedTickUs = std::numeric_limits<uint64_t>::max();
        }
        else
        {
            _elapsedTickUs += deltaUs;
        }

        const uint64_t elapsedMs = _elapsedTickUs / 1'000u;
        if (elapsedMs > std::numeric_limits<uint64_t>::max() - _tickEpochMs)
        {
            return std::numeric_limits<uint64_t>::max();
        }

        return _tickEpochMs + elapsedMs;
    }

    [[nodiscard]] TickTiming ComputeTickTiming(uint64_t rawDeltaUs) noexcept
    {
        const uint64_t callbackDeltaUs = _clock.SmoothDeltaUs(rawDeltaUs, _frameBudget);
        const uint64_t legacyGapMs     = rawDeltaUs / 1'000u;
        return TickTiming{
            rawDeltaUs,
            callbackDeltaUs,
            legacyGapMs,
            legacyGapMs > 20u,
        };
    }

    // The window procedure enters DxUi work only under an active failure scope. Its own teardown (WM_DESTROY and
    // WM_NCDESTROY, including the destruction of a window left alive until thread-local destruction) and a timer message
    // that arrives after the module's failure host has gone do allocation-free, callback-free cleanup without one, so a
    // window outliving its hosts can never turn an ordinary thread or process exit into a MissingHost failure.
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) noexcept
    {
        if (msg == WM_NCCREATE)
        {
            const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lp);
            auto* self     = static_cast<AnimationDispatcher*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        auto* self = reinterpret_cast<AnimationDispatcher*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCDESTROY)
        {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            if (self)
                self->OnWindowDestroyed();
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        if (msg != WM_TIMER)
            return DefWindowProcW(hwnd, msg, wp, lp);

        const Failure::Scope failureScope(std::nothrow);
        if (! self || ! failureScope.IsActive())
        {
            KillTimer(hwnd, static_cast<UINT_PTR>(wp));
            return 0;
        }
        const auto timerId = static_cast<UINT_PTR>(wp);
        if (timerId == kTimerId || timerId == kDeadlineTimerId)
        {
            self->OnTimerTick(timerId == kDeadlineTimerId);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    void OnWindowDestroyed() noexcept
    {
        _timerRunning  = false;
        _deadlineArmed = false;
        ResetClock();
        _hwnd.release();
    }

    void EnsureWindow() noexcept
    {
        if (_hwnd && IsWindow(_hwnd.get()))
        {
            return;
        }

        const HINSTANCE instance = wil::GetModuleInstanceHandle();
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = &AnimationDispatcher::WndProcThunk;
        wc.hInstance     = instance;
        wc.lpszClassName = kWindowClassName;
        // The class belongs to this archive's consuming module. Multiple UI threads may
        // register it concurrently; an existing registration needs no shared atom cache.
        if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return;
        }

        HWND hwnd = CreateWindowExW(0, kWindowClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, this);

        if (! hwnd)
        {
            return;
        }

        _hwnd.reset(hwnd);
        _timerRunning  = false;
        _deadlineArmed = false;
    }

    void ResetClock() noexcept
    {
        _lastTickMs        = 0u;
        _tickEpochMs       = 0u;
        _elapsedTickUs     = 0u;
        _lastTickTimestamp = {};
        _hasTickTimestamp  = false;
    }

    void EnsureTimerRunning() noexcept
    {
        if (_timerRunning || ! _hwnd)
        {
            return;
        }

        const UINT_PTR timer = SetCoalescableTimer(_hwnd.get(), kTimerId, kFrameIntervalMs, nullptr, TIMERV_DEFAULT_COALESCING);
        if (timer != 0)
        {
            _timerRunning = true;
        }
    }

    void StopTimer() noexcept
    {
        if (_timerRunning && _hwnd)
        {
            KillTimer(_hwnd.get(), kTimerId);
        }
        _timerRunning = false;
        ResetClock();
    }

    void StopDeadlineTimer() noexcept
    {
        if (_deadlineArmed && _hwnd)
        {
            KillTimer(_hwnd.get(), kDeadlineTimerId);
        }
        _deadlineArmed       = false;
        _armedDeadlineTickMs = 0u;
    }

    void ArmDeadlineTimer(uint64_t dueTickMs) noexcept
    {
        if (! _hwnd || (_deadlineArmed && _armedDeadlineTickMs == dueTickMs))
        {
            return;
        }
        const uint64_t nowTickMs = GetCurrentTickMs();
        const uint64_t delayMs   = dueTickMs > nowTickMs ? dueTickMs - nowTickMs + kDeadlineSlackMs : 0u;
        const UINT elapseMs      = static_cast<UINT>((std::min)(delayMs, static_cast<uint64_t>(USER_TIMER_MAXIMUM)));
        if (SetCoalescableTimer(_hwnd.get(), kDeadlineTimerId, elapseMs, nullptr, TIMERV_DEFAULT_COALESCING) != 0)
        {
            _deadlineArmed       = true;
            _armedDeadlineTickMs = dueTickMs;
        }
    }

    // One timer arrangement for every subscriber of the thread: the periodic timer while any of them needs every frame,
    // else the deadline timer for the earliest deadline, else nothing (and the window goes when nobody is subscribed).
    void EnsureTimerState() noexcept
    {
        if (_subscriptions.empty() && _pendingAdds.empty())
        {
            StopTimer();
            StopDeadlineTimer();
            // Native hosts keep the window warm between gestures. The final host
            // releases it while the module and process failure reporter are alive.
            // A tick that detaches its host reaches this point only after callbacks.
            if (! _retainIdleWindow)
                _hwnd.reset();
            return;
        }

        bool continuous      = false;
        bool hasDeadline     = false;
        uint64_t earliestDue = 0u;
        const auto visit     = [&](const std::vector<Subscription>& list) noexcept
        {
            for (const auto& entry : list)
            {
                if (! IsActive(entry))
                    continue;
                if (entry.wake.kind == WakeKind::Continuous)
                    continuous = true;
                else if (entry.wake.kind == WakeKind::Deadline && (! hasDeadline || entry.wake.dueTickMs < earliestDue))
                {
                    hasDeadline = true;
                    earliestDue = entry.wake.dueTickMs;
                }
            }
        };
        visit(_subscriptions);
        visit(_pendingAdds);

        if (continuous)
        {
            // Every periodic tick also serves the deadlines that have come due.
            StopDeadlineTimer();
            EnsureTimerRunning();
            return;
        }
        StopTimer();
        if (hasDeadline)
            ArmDeadlineTimer(earliestDue);
        else
            StopDeadlineTimer();
    }

    void GarbageCollect() noexcept
    {
        std::erase_if(_subscriptions, [](const Subscription& entry) noexcept { return ! IsActive(entry); });
        std::erase_if(_pendingAdds, [](const Subscription& entry) noexcept { return ! IsActive(entry); });
    }

    void AppendPendingAdds() noexcept
    {
        if (_pendingAdds.empty())
        {
            return;
        }

        _subscriptions.reserve(_subscriptions.size() + _pendingAdds.size());
        for (const auto& entry : _pendingAdds)
        {
            if (IsActive(entry))
            {
                _subscriptions.push_back(entry);
            }
        }

        _pendingAdds.clear();
    }

    [[nodiscard]] uint64_t AdvancePeriodicClock() noexcept
    {
        const DxUi::FrameTimestamp timestamp = _clock.Now();
        const bool hadPreviousTick           = _hasTickTimestamp;
        if (! _hasTickTimestamp)
        {
            // The first tick after idle starts the timeline at the real time it runs: no frame is assumed to have passed.
            _tickEpochMs       = GetTickCount64();
            _lastTickTimestamp = timestamp;
            _hasTickTimestamp  = true;
            _elapsedTickUs     = 0u;
            _lastTickMs        = _tickEpochMs;
        }

        const uint64_t rawDeltaUs = hadPreviousTick ? _clock.ElapsedUs(_lastTickTimestamp, timestamp) : 0u;
        const TickTiming timing   = ComputeTickTiming(rawDeltaUs);
        const uint64_t now        = AddElapsedTickUs(timing.callbackDeltaUs);
        if (hadPreviousTick)
        {
            Debug::Perf::EmitDurationUs(L"dxui.animation.tick_delta_us", timing.rawDeltaUs);
            Debug::Perf::EmitDurationUs(L"dxui.animation.jitter_us", AbsoluteDifference(timing.rawDeltaUs, _frameBudget.refreshPeriodUs));
            Debug::Perf::EmitValue(L"dxui.animation.tick_gap_ms", timing.legacyGapMs);
            if (timing.legacyOverrun)
            {
                Debug::Perf::EmitCounter(L"dxui.animation.tick_overrun");
            }
        }
        _lastTickMs        = now;
        _lastTickTimestamp = timestamp;
        return now;
    }

    void OnTimerTick(bool deadlineTimer) noexcept
    {
        const Failure::Scope failureScope;
        if (deadlineTimer)
        {
            // One-shot: the arrangement after this wake arms the next deadline, if any.
            ++::DxUi::Detail::Work().wakes.deadlineTimer;
            if (_hwnd)
                KillTimer(_hwnd.get(), kDeadlineTimerId);
            _deadlineArmed       = false;
            _armedDeadlineTickMs = 0u;
            if (::DxUi::Detail::Trace::Listening()) [[unlikely]]
                ::DxUi::Detail::Trace::Wake(::DxUi::Detail::Trace::WakeSource::AnimationTimer);
        }
        else
        {
            ++::DxUi::Detail::Work().wakes.animationTimer;
            if (::DxUi::Detail::Trace::Listening()) [[unlikely]]
                ::DxUi::Detail::Trace::Wake(::DxUi::Detail::Trace::WakeSource::AnimationTimer);
        }
        if (_subscriptions.empty())
        {
            GarbageCollect();
            AppendPendingAdds();
            EnsureTimerState();
            return;
        }

        // A deadline wake reads the idle clock; a periodic tick advances the frame timeline.
        const uint64_t now = (deadlineTimer && ! _timerRunning) ? GetCurrentTickMs() : AdvancePeriodicClock();

        _inTick = true;
        // Index loop: a callback may unsubscribe (marking entries) or subscribe (deferred to _pendingAdds).
        for (size_t index = 0u; index < _subscriptions.size(); ++index)
        {
            Subscription& entry = _subscriptions[index];
            if (! IsActive(entry))
            {
                continue;
            }
            if (entry.tick)
            {
                const TickCallback callback = entry.tick;
                void* const context         = entry.context;
                const uint64_t id           = entry.id;
                const bool keep             = callback(context, now);
                if (! keep)
                    MarkPendingRemove(_subscriptions, id);
                continue;
            }
            const bool due = entry.wake.kind == WakeKind::Continuous || (entry.wake.kind == WakeKind::Deadline && entry.wake.dueTickMs <= now);
            if (! due)
                continue;
            const ScheduleCallback callback = entry.schedule;
            void* const context             = entry.context;
            const uint64_t id               = entry.id;
            const Wake next                 = callback(context, now);
            // The vector may have been cleared by a reentrant Shutdown; look the entry up again.
            if (Subscription* const current = Find(id))
                current->wake = next;
        }
        _inTick = false;

        GarbageCollect();
        AppendPendingAdds();
        Debug::Perf::EmitValue(L"dxui.animation.active_count", static_cast<uint64_t>(CountActiveSubscriptions(_subscriptions)));
        EnsureTimerState();
    }

private:
    wil::unique_hwnd _hwnd;
    bool _retainIdleWindow        = false;
    bool _timerRunning            = false;
    bool _deadlineArmed           = false;
    bool _inTick                  = false;
    uint64_t _armedDeadlineTickMs = 0u;
    uint64_t _nextSubscriptionId  = 1;
    uint64_t _lastTickMs          = 0u;
    uint64_t _tickEpochMs         = 0u;
    uint64_t _elapsedTickUs       = 0u;
    DxUi::FrameClock _clock;
    DxUi::FrameBudget _frameBudget{kTargetFrameUs, kHitchClampUs};
    DxUi::FrameTimestamp _lastTickTimestamp{};
    bool _hasTickTimestamp = false;
    std::vector<Subscription> _subscriptions;
    std::vector<Subscription> _pendingAdds;
};
} // namespace DxUi::Ui
