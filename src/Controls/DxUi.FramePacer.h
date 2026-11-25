#pragma once

// Native frame pacing (performance and motion plan item W3.6; Specs/Rendering/Rendering_Win32Host.md, "Frame pacing").
//
// A window host's swap chain is created with DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT at latency 1. Its waitable
// object behaves as a semaphore: it starts with one unit, and every retired present adds one (observed on WARP and hardware,
// in both presentation modes, shown or hidden; ResizeBuffers neither adds nor removes a unit). A unit the host holds is its
// credit: the next Present spends it. The host never presents without a credit, so a present never queues behind another,
// and it never waits on the object while it holds one, so nothing can wait for a unit that no present will return.
//
// Only the UI thread reads or writes the credit. When the host needs a frame and holds no credit, it arms one thread-pool
// wait (the process default pool; DxUi starts no thread) on the object. The wait's callback runs on a pool thread and does
// exactly three things: reads the request it was armed for, records that the request was signaled, and posts one paced-frame
// message carrying that request's identity in wParam through the posted-payload helper. It touches no host, allocates
// nothing and takes no lock. There is no heap payload, so a queued paced-frame message owns nothing and teardown has nothing
// to drain from the payload registry: a message that outlives its request is recognized by its identity and ignored.
//
// Request identities come from one process-wide counter and are never reused. Cancelling a wait (SetThreadpoolWait to null,
// then WaitForThreadpoolWaitCallbacks without cancelling pending callbacks) lets a callback whose wait was already satisfied
// finish, so a unit the wait took is harvested as a credit instead of lost; the message it posted is then stale. Teardown is
// always cancel, drain, close the handle, then release the swap chain; the members below are declared so that destruction
// follows the same order on every path.
//
// Only a Present that returned S_OK spends the credit; any other result keeps it (never more than one credit; an excess unit a
// kept credit's frame returns is absorbed before the next present).
//
// Invariant (checked after each pacer transition in Debug, ControlHost's CheckFramePacerInvariant): a deferred paint always
// has a wake, which is an armed wait (whose message may already be posted) or the paced frame rendering it now. A path that
// takes the credit while a paint is deferred (a synchronous render, a test seam) hands the deferred region back to the
// window's update region, where the next WM_PAINT renders it with that credit. A pending pointer move (W3.7) is held to the
// same invariant: it has an armed wait, the paced frame dispatching it now, or a posted pointer-move flush.
#include <atomic>
#include <cstdint>

#include <dxgi1_6.h>
#include <windows.h>

#pragma warning(push)
// WIL: C4625 (copy ctor deleted), C4626 (copy assign deleted), C5026 (move ctor deleted), C5027 (move assign deleted)
#pragma warning(disable : 4625 4626 5026 5027 28182)
#include <wil/com.h>
#include <wil/resource.h>
#pragma warning(pop)

#include "Support/PostedPayload.h"
#include "Support/WindowMessages.h"
#include "Support/WorkCounters.h"

namespace DxUi::Detail
{
// The paced-frame message's type tag for the posted-payload helper: it carries no payload, only the request in wParam.
struct PacedFramePost final
{
};

// What the pool callback reads and writes: the only state shared with a pool thread.
struct FramePacerSignal final
{
    FramePacerSignal()                                   = default;
    FramePacerSignal(const FramePacerSignal&)            = delete;
    FramePacerSignal(FramePacerSignal&&)                 = delete;
    FramePacerSignal& operator=(const FramePacerSignal&) = delete;
    FramePacerSignal& operator=(FramePacerSignal&&)      = delete;
    ~FramePacerSignal()                                  = default;

    HWND hwnd    = nullptr;
    UINT message = 0u;
    std::atomic<uint64_t> armed{0u};
    std::atomic<uint64_t> signaled{0u};
};

// A wait that sees no retired frame for this long (a stalled compositor: display off, a locked or disconnected session, a
// remote session that stops composing) reports a timeout: the frame stays deferred and the wait is armed again, so a pending
// frame costs at most one wake per period and nothing is ever presented into a full queue.
inline constexpr DWORD kFramePacerWaitTimeoutMs = 250u;
// The longest a synchronous render whose caller reads the presented result (the first frame at show, a test's render)
// waits for a credit. It never waits in WM_PAINT or WM_SIZE.
inline constexpr DWORD kFramePacerSynchronousWaitMs = 250u;

inline std::atomic<uint64_t>& FramePacerRequestCounter() noexcept
{
    static std::atomic<uint64_t> counter{0u};
    return counter;
}

inline void CALLBACK FramePacerWaitCallback(PTP_CALLBACK_INSTANCE, void* context, PTP_WAIT, TP_WAIT_RESULT result) noexcept
{
    auto* const signal     = static_cast<FramePacerSignal*>(context);
    const uint64_t request = signal->armed.load(std::memory_order_acquire);
    // Only a satisfied wait took a unit; a timed-out one leaves `signaled` behind, which is how the receiver tells them apart.
    if (result == WAIT_OBJECT_0)
        signal->signaled.store(request, std::memory_order_release);
    // The helper's payload-free form: identity in wParam, nothing registered, nothing to drain.
    static_cast<void>(PostMessagePayload<PacedFramePost>(signal->hwnd, signal->message, static_cast<WPARAM>(request), std::unique_ptr<PacedFramePost>{}));
}

class FramePacer final
{
public:
    enum class FrameMessage : uint8_t
    {
        Stale,
        Signaled,
        TimedOut,
    };

    FramePacer() noexcept                    = default;
    FramePacer(const FramePacer&)            = delete;
    FramePacer(FramePacer&&)                 = delete;
    FramePacer& operator=(const FramePacer&) = delete;
    FramePacer& operator=(FramePacer&&)      = delete;
    ~FramePacer()
    {
        DrainWait();
        // At process exit the pool's threads are already gone: nothing can call back, and waiting for callbacks (the wait's
        // own destructor waits for them) could hang.
        if (wil::ProcessShutdownInProgress())
        {
            static_cast<void>(_wait.release());
            static_cast<void>(_waitable.release());
        }
    }

    // A new swap chain: its waitable object at latency 1, and the unit it starts with as the first credit. False when the
    // chain offers none (the host then runs unpaced).
    [[nodiscard]] bool BindChain(HWND hwnd, IDXGISwapChain1* chain) noexcept
    {
        ReleaseChain();
        _signal.hwnd    = hwnd;
        _signal.message = WndMsg::WindowHostPacedFrame().value;
        wil::com_ptr_nothrow<IDXGISwapChain2> chain2;
        if (! chain || ! _signal.message || FAILED(chain->QueryInterface(IID_PPV_ARGS(chain2.put()))) || ! chain2 || FAILED(chain2->SetMaximumFrameLatency(1u)))
            return false;
        _waitable.reset(chain2->GetFrameLatencyWaitableObject());
        if (! _waitable)
            return false;
        if (! _wait)
        {
            _wait.reset(CreateThreadpoolWait(&FramePacerWaitCallback, &_signal, nullptr));
            if (! _wait)
            {
                _waitable.reset();
                return false;
            }
        }
        static_cast<void>(TryAcquire());
        return true;
    }

    // Cancel, drain, close the handle: the swap chain may be released after this returns.
    void ReleaseChain() noexcept
    {
        DrainWait();
        _credit = false;
        framing = false;
        _waitable.reset();
    }

    [[nodiscard]] bool IsPaced() const noexcept
    {
        return static_cast<bool>(_waitable);
    }
    [[nodiscard]] bool HoldsWaitable() const noexcept
    {
        return static_cast<bool>(_waitable) || _waitArmed;
    }
    [[nodiscard]] bool HasCredit() const noexcept
    {
        return _credit;
    }
    [[nodiscard]] bool IsWaitArmed() const noexcept
    {
        return _waitArmed;
    }

    // A credit already held, or one unit taken without waiting when no wait is armed.
    [[nodiscard]] bool TryAcquire() noexcept
    {
        if (_credit || ! _waitable)
            return _credit || ! _waitable;
        if (_waitArmed)
            return false;
        if (WaitForSingleObject(_waitable.get(), 0u) == WAIT_OBJECT_0)
            _credit = true;
        return _credit;
    }

    // Arms the wait when a frame is wanted and no credit is held. Never while a credit is held.
    bool Arm() noexcept
    {
        if (! _waitable || ! _wait || _credit || _waitArmed)
            return _waitArmed;
        _armedRequest = FramePacerRequestCounter().fetch_add(1u, std::memory_order_relaxed) + 1u;
        _signal.armed.store(_armedRequest, std::memory_order_release);
        // Relative due time in 100 ns units.
        ULARGE_INTEGER due{};
        due.QuadPart = static_cast<ULONGLONG>(-static_cast<LONGLONG>(_waitTimeoutMs) * 10'000LL);
        FILETIME timeout{due.LowPart, due.HighPart};
        SetThreadpoolWait(_wait.get(), _waitable.get(), &timeout);
        _waitArmed = true;
        return true;
    }

    // Stops an armed wait; a unit its callback took becomes the credit, and the message it posted becomes stale.
    void Cancel() noexcept
    {
        if (! _waitArmed)
            return;
        DrainWait();
        if (_signal.signaled.load(std::memory_order_acquire) == _cancelledRequest && _cancelledRequest != 0u)
            _credit = true;
    }

    // A synchronous render whose caller reads the presented result: waits at most timeoutMs for a credit.
    [[nodiscard]] bool AcquireWithin(DWORD timeoutMs) noexcept
    {
        Cancel();
        if (TryAcquire())
            return true;
        if (WaitForSingleObject(_waitable.get(), timeoutMs) == WAIT_OBJECT_0)
            _credit = true;
        return _credit;
    }

    // Before every Present or Present1 call: one made without a credit is counted (the gate requires none). A unit in the
    // object while the credit is held is one too many (a credit kept for a frame that queued after all, see AfterPresent):
    // it is taken and dropped, so at most one frame is ever queued behind the one shown.
    void BeforePresent() noexcept
    {
        if (_waitable && ! _credit)
            ++Work().frame.presentsWithoutCredit;
        if (_waitable && _credit && ! _waitArmed && WaitForSingleObject(_waitable.get(), 0u) == WAIT_OBJECT_0)
            ++Work().frame.pacerExcessUnits;
    }
    // After it: only S_OK spends the credit. A failure queued nothing and no unit will come back for it. A success status
    // such as DXGI_STATUS_OCCLUDED may or may not have queued a frame: spending the credit on a frame that returns no unit
    // would stop the window painting for good, keeping it for one that does costs one excess unit, absorbed by BeforePresent.
    // So any result other than S_OK keeps the credit (a device loss then releases the chain, and the credit with it).
    void AfterPresent(HRESULT result) noexcept
    {
        if (result == S_OK)
            _credit = false;
    }

    [[nodiscard]] FrameMessage OnFrameMessage(WPARAM request) noexcept
    {
        if (! _waitArmed || request == 0u || static_cast<uint64_t>(request) != _armedRequest)
        {
            ++Work().frame.pacerStaleFrames;
            return FrameMessage::Stale;
        }
        _waitArmed    = false;
        _armedRequest = 0u;
        if (_signal.signaled.load(std::memory_order_acquire) != static_cast<uint64_t>(request))
        {
            ++Work().frame.pacerWaitTimeouts;
            return FrameMessage::TimedOut;
        }
        _credit = true;
        return FrameMessage::Signaled;
    }

    // Deferred paint: the union of the regions WM_PAINT validated while no credit was held.
    void Defer(const RECT& rect) noexcept
    {
        if (! _deferred)
            _deferredRect = rect;
        else
            UnionRect(&_deferredRect, &_deferredRect, &rect);
        _deferred = true;
    }
    [[nodiscard]] bool HasDeferred() const noexcept
    {
        return _deferred;
    }
    [[nodiscard]] RECT TakeDeferred() noexcept
    {
        _deferred = false;
        return _deferredRect;
    }
    void DropDeferred() noexcept
    {
        _deferred = false;
    }

    // Whether the host's schedule asks for every frame and the pacer serves it.
    bool continuous       = false;
    bool framing          = false; // a paced frame is running: a deferred paint it holds is being rendered now
    bool skipTimerArmed   = false;
    bool timelineAcquired = false;
    // W3.7: a pointer move that arrived while no credit was free waits for the next paced frame. Like a deferred paint it
    // always has a wake: the armed wait, the paced frame dispatching it now, or a posted pointer-move flush (pointerFlushPosted)
    // when the chain that owned the wait was released.
    bool pointerMovePending = false;
    bool pointerFlushPosted = false;
    // The compositor's refresh period, in QPC ticks; 0 until a frame read it.
    uint64_t periodQpc  = 0u;
    uint64_t lastSample = 0u; // the QPC time the last paced sample stands for

    // Tests only: the waitable object a test owns (duplicated; the pacer closes its copy), and the wait's timeout.
    void ReplaceWaitableForTest(HANDLE waitable) noexcept
    {
        DrainWait();
        _credit     = false;
        HANDLE copy = nullptr;
        if (waitable && DuplicateHandle(GetCurrentProcess(), waitable, GetCurrentProcess(), &copy, 0u, FALSE, DUPLICATE_SAME_ACCESS))
            _waitable.reset(copy);
        else
            _waitable.reset();
    }
    void SetWaitTimeoutForTest(DWORD timeoutMs) noexcept
    {
        _waitTimeoutMs = timeoutMs == 0u ? kFramePacerWaitTimeoutMs : timeoutMs;
    }
    [[nodiscard]] uint64_t ArmedRequestForTest() const noexcept
    {
        return _armedRequest;
    }

private:
    // Every drain passes here (Cancel, ReleaseChain, the test seams, the destructor).
    void DrainWait() noexcept
    {
        _cancelledRequest = 0u;
        if (! _waitArmed || ! _wait)
        {
            _waitArmed = false;
            return;
        }
        // At process exit the pool's threads are already gone: nothing can call back, and waiting for callbacks could hang.
        // The wait is abandoned (the destructor then releases it unclosed); a unit it may have taken is not harvested.
        if (wil::ProcessShutdownInProgress())
        {
            _waitArmed    = false;
            _armedRequest = 0u;
            return;
        }
        SetThreadpoolWait(_wait.get(), nullptr, nullptr);
        // FALSE: a callback whose wait was satisfied still runs, so the unit it took is recorded, never lost.
        WaitForThreadpoolWaitCallbacks(_wait.get(), FALSE);
        _cancelledRequest = _armedRequest;
        _waitArmed        = false;
        _armedRequest     = 0u;
    }

    // Destroyed last to first: the wait (cancel and drain), then the handle, then the signal its callback reads.
    FramePacerSignal _signal;
    wil::unique_handle _waitable;
    wil::unique_threadpool_wait _wait;
    uint64_t _armedRequest     = 0u;
    uint64_t _cancelledRequest = 0u;
    RECT _deferredRect{};
    DWORD _waitTimeoutMs = kFramePacerWaitTimeoutMs;
    bool _credit         = false;
    bool _waitArmed      = false;
    bool _deferred       = false;
};
} // namespace DxUi::Detail
