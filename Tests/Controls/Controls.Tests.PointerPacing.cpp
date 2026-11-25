#include "Controls.Tests.PointerPacing.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.GridScrollScenario.h"
#include "Support/Trace.h"
#include "Support/WorkCounters.h"

#include <dwmapi.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxgi.lib")

// W3.7's gate, every clause that needs no display at a chosen refresh rate: on WARP, with the frame pacer's test seams (a
// test-owned frame-latency object decides when a credit is free), the work counters and the trace provider's test sink. The
// hardware clause (input to present within one refresh interval at p99) is TestPointerPacingInputToPresentOnHardware.
// Nothing here takes focus or moves the pointer: messages are sent or posted to the test's own non-activated windows.
namespace
{
using DxUi::Detail::WorkCounters;
using DxUi::Detail::WorkCounterScope;
namespace Trace = DxUi::Detail::Trace;

// The test windows sit off every monitor, so the system posts WM_MOUSELEAVE as soon as the host's TrackMouseEvent asks for it
// (as the performance-fixture runner notes): dispatching it would turn every move into a leave, which dispatches a pending move
// at once. A test that needs a leave sends its own.
void PumpOnce()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
    {
        if (message.message == WM_MOUSELEAVE)
            continue;
        static_cast<void>(DispatchQueuedMessageForTest(message));
    }
}

// Pumps until the condition holds. No wall-time bound: a condition that never holds ends the run at the test watchdog.
template <class Condition> void PumpUntil(Condition&& reached)
{
    while (! reached())
    {
        PumpOnce();
        if (reached())
            return;
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, 20u, QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
}

// A control that records what reaches it, in order.
class MoveProbe final : public DxUi::Control
{
public:
    enum class Kind : uint8_t
    {
        Move,
        Down,
        Up,
        Wheel,
        Key,
        Tick,
    };
    struct Entry
    {
        Kind kind{};
        D2D1_POINT_2F point{};    // the event's own point (a move's, a button's), or the latest move seen (a tick, a key)
        D2D1_POINT_2F lastMove{}; // the latest move the probe had seen when the event reached it
    };

    MoveProbe()                            = default;
    MoveProbe(const MoveProbe&)            = delete;
    MoveProbe& operator=(const MoveProbe&) = delete;
    MoveProbe(MoveProbe&&)                 = delete;
    MoveProbe& operator=(MoveProbe&&)      = delete;
    ~MoveProbe() override                  = default;

    void Paint(DxUi::ControlHost&) const override
    {
        if (painting)
            ++nestedPaints;
        painting = true;
        ++paints;
        if (onPaint)
            onPaint();
        painting = false;
    }
    bool OnMouseMove(DxUi::ControlHost& host, D2D1_POINT_2F point, UINT) override
    {
        ++moves;
        lastMove = point;
        log.push_back({Kind::Move, point, point});
        Invalidate(host); // every move changes what the probe shows: each dispatched move owes a frame
        return true;
    }
    bool OnMouseDown(DxUi::ControlHost& host, D2D1_POINT_2F point, bool, UINT) override
    {
        log.push_back({Kind::Down, point, lastMove});
        if (onDown)
            onDown(host);
        return true;
    }
    bool OnMouseUp(DxUi::ControlHost&, D2D1_POINT_2F point, bool, UINT) override
    {
        log.push_back({Kind::Up, point, lastMove});
        return true;
    }
    bool OnMouseWheel(DxUi::ControlHost&, D2D1_POINT_2F point, float, UINT) override
    {
        log.push_back({Kind::Wheel, point, lastMove});
        return true;
    }
    bool OnKeyDown(DxUi::ControlHost&, UINT, UINT) override
    {
        ++keys;
        log.push_back({Kind::Key, lastMove, lastMove});
        return true;
    }
    bool Tick(DxUi::ControlHost& host, uint64_t) override
    {
        log.push_back({Kind::Tick, lastMove, lastMove});
        Invalidate(host); // presents every frame, so the next one waits for a credit
        return true;
    }
    void Repaint(DxUi::ControlHost& host) const
    {
        Invalidate(host);
    }
    [[nodiscard]] DxUi::WindowHostCursorKind ResolveCursorKind(DxUi::ControlHost&, D2D1_POINT_2F point) const noexcept override
    {
        return point.x < 100.0f ? DxUi::WindowHostCursorKind::Hand : DxUi::WindowHostCursorKind::IBeam;
    }

    size_t moves = 0u;
    size_t keys  = 0u;
    D2D1_POINT_2F lastMove{-1.0f, -1.0f};
    std::vector<Entry> log;
    std::function<void(DxUi::ControlHost&)> onDown;
    std::function<void()> onPaint;
    mutable size_t paints       = 0u;
    mutable size_t nestedPaints = 0u;
    mutable bool painting       = false;
};

// A shown, non-activated WARP window host whose probe fills its client area, its first frame presented (paced).
struct PacedProbeWindow final
{
    PacedProbeWindow() : window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true)
    {
        auto root = std::make_unique<DxUi::Panel>();
        probe     = root->AddChild<MoveProbe>();
        probe->SetBounds(D2D1::RectF(0.0f, 0.0f, 2000.0f, 2000.0f));
        window.Host().SetRoot(std::move(root));
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        Require(window.Host().RenderInitialFrameForShow(), "the paced host renders its first frame");
        Require(window.Host().DebugGetFramePacerStateForTest().paced, "a WARP swap chain is paced");
        Settle();
    }
    PacedProbeWindow(const PacedProbeWindow&)            = delete;
    PacedProbeWindow& operator=(const PacedProbeWindow&) = delete;
    PacedProbeWindow(PacedProbeWindow&&)                 = delete;
    PacedProbeWindow& operator=(PacedProbeWindow&&)      = delete;
    ~PacedProbeWindow()                                  = default;

    [[nodiscard]] DxUi::WindowHost& Host() noexcept
    {
        return window.Host();
    }
    [[nodiscard]] HWND Hwnd() const noexcept
    {
        return window.Hwnd();
    }
    [[nodiscard]] DxUi::ControlHost::DebugFramePacerState State() noexcept
    {
        return Host().DebugGetFramePacerStateForTest();
    }
    // Nothing armed, nothing deferred, no move pending.
    void Settle()
    {
        PumpUntil([this]
        {
            const auto state = State();
            return ! state.waitArmed && ! state.deferred && ! state.continuous && ! state.skipTimerArmed && ! state.pointerMovePending &&
                   GetUpdateRect(Hwnd(), nullptr, FALSE) == FALSE;
        });
    }
    // Hands the pacer a test-owned frame-latency object with no unit: no credit is free until the test releases one.
    void TakeFrameLatencyObject(DWORD waitTimeoutMs = 0u)
    {
        units.reset(CreateSemaphoreW(nullptr, 0, 64, nullptr));
        Require(units.is_valid(), "the test's frame-latency object");
        Host().DebugSetFramePacerWaitableForTest(units.get(), waitTimeoutMs);
        Require(! State().credit, "the pacer holds no credit");
    }
    void ReleaseUnit()
    {
        Require(ReleaseSemaphore(units.get(), 1, nullptr) != FALSE, "a frame retires");
    }
    void Move(int x, int y) const
    {
        static_cast<void>(SendMessageW(Hwnd(), WM_MOUSEMOVE, 0u, MAKELPARAM(x, y)));
    }
    [[nodiscard]] D2D1_POINT_2F Dip(int x, int y)
    {
        return D2D1::Point2F(Host().PixelsToDip(static_cast<float>(x)), Host().PixelsToDip(static_cast<float>(y)));
    }
    [[nodiscard]] LPARAM Screen(int x, int y) const
    {
        POINT point{x, y};
        static_cast<void>(ClientToScreen(Hwnd(), &point));
        return MAKELPARAM(static_cast<short>(point.x), static_cast<short>(point.y));
    }

    AttachedHostWindow window;
    MoveProbe* probe = nullptr;
    wil::unique_handle units;
};

bool SamePoint(D2D1_POINT_2F a, D2D1_POINT_2F b) noexcept
{
    return a.x == b.x && a.y == b.y;
}

struct RecordCapture final
{
    std::vector<Trace::Record> records;
    size_t overflow = 0u;
    explicit RecordCapture(size_t capacity)
    {
        records.reserve(capacity);
    }
    static void Sink(void* context, const Trace::Record& record) noexcept
    {
        auto& capture = *static_cast<RecordCapture*>(context);
        if (capture.records.size() == capture.records.capacity())
        {
            ++capture.overflow;
            return;
        }
        capture.records.push_back(record);
    }
};

class ScopedSink final
{
public:
    explicit ScopedSink(RecordCapture& capture) noexcept
    {
        Trace::InstallSinkForTest(&RecordCapture::Sink, &capture);
    }
    ~ScopedSink()
    {
        Trace::InstallSinkForTest(nullptr, nullptr);
    }
    ScopedSink(const ScopedSink&)            = delete;
    ScopedSink& operator=(const ScopedSink&) = delete;
    ScopedSink(ScopedSink&&)                 = delete;
    ScopedSink& operator=(ScopedSink&&)      = delete;
};

// Between two consecutive presents (and before the first), at most one dispatch that left a frame owed.
[[nodiscard]] size_t MostFrameOwingDispatchesPerPresent(const std::vector<Trace::Record>& records) noexcept
{
    size_t most    = 0u;
    size_t current = 0u;
    for (const Trace::Record& record : records)
    {
        if (record.event == Trace::Event::Present)
            current = 0u;
        else if (record.event == Trace::Event::InputDispatched && record.owesFrame)
            most = std::max(most, ++current);
    }
    return most;
}
} // namespace

// A move that arrives while the host holds its credit and owes no frame is dispatched at once, and its frame renders at the
// next message the window retrieves: no paced frame, timer or animation tick is waited for.
void TestPointerPacingIdleMoveDispatchesAtOnce()
{
    PacedProbeWindow paced;
    Require(AwaitFrameCreditForTest(paced.Host()), "the host holds its credit");
    const WorkCounterScope work;
    paced.Move(40, 30);
    Require(paced.probe->moves == 1u && SamePoint(paced.probe->lastMove, paced.Dip(40, 30)) && ! paced.State().pointerMovePending,
            "an idle move is dispatched on arrival");
    Require(GetUpdateRect(paced.Hwnd(), nullptr, FALSE) != FALSE, "its dispatch asks for a frame");
    PumpOnce();
    const WorkCounters delta = work.Delta();
    Require(delta.frame.pointerMovesCoalesced == 0u && delta.frame.pointerMoveDispatches == 1u, "nothing waited");
    Require(delta.frame.presents == 1u && delta.frame.presentsWithoutCredit == 0u, "its frame rendered and presented with the credit");
    Require(delta.wakes.pacedFrame == 0u && delta.wakes.pacerTimer == 0u && delta.wakes.animationTimer == 0u && delta.wakes.deadlineTimer == 0u,
            "without waiting for a paced frame, a timer or an animation tick");
}

// Moves posted to a non-activated window while a frame is in flight coalesce: one dispatch per presented frame, at the latest
// position, standing for every move it absorbed (the trace's InputDispatched event carries the first and the latest).
void TestPointerPacingAtMostOneDispatchPerPresentedFrame()
{
    PacedProbeWindow paced;
    paced.TakeFrameLatencyObject();
    constexpr int kRounds = 6;
    constexpr int kMoves  = 40;
    RecordCapture capture(1u << 14u);
    const WorkCounterScope work;
    {
        const ScopedSink sink(capture);
        for (int round = 0; round < kRounds; ++round)
        {
            const size_t movesBefore = paced.probe->moves;
            for (int move = 0; move < kMoves; ++move)
                Require(PostMessageW(paced.Hwnd(), WM_MOUSEMOVE, 0u, MAKELPARAM(10 + move + round, 20 + round)) != FALSE, "a move is posted");
            PumpOnce();
            Require(paced.probe->moves == movesBefore && paced.State().pointerMovePending && paced.State().waitArmed,
                    "while a frame is in flight the moves wait, with an armed wait as their wake");
            paced.ReleaseUnit();
            PumpUntil([&] { return ! paced.State().pointerMovePending && work.Delta().frame.presents == static_cast<uint64_t>(round + 1); });
            Require(paced.probe->moves == movesBefore + 1u && SamePoint(paced.probe->lastMove, paced.Dip(10 + kMoves - 1 + round, 20 + round)),
                    "the next paced frame dispatches the latest position once");
        }
    }
    const WorkCounters delta = work.Delta();
    std::cout << "    [W3.7] " << kRounds * kMoves << " moves: " << delta.frame.pointerMoveDispatches << " dispatches, " << delta.frame.presents
              << " presents, " << delta.frame.pointerMovesCoalesced << " coalesced\n";
    Require(capture.overflow == 0u, "the capture holds every record");
    Require(delta.frame.pointerMoveDispatches == static_cast<uint64_t>(kRounds) && delta.frame.presents == static_cast<uint64_t>(kRounds) &&
                delta.frame.pointerMovesCoalesced == static_cast<uint64_t>(kRounds * kMoves),
            "one dispatch per presented frame");
    Require(delta.frame.presentsWithoutCredit == 0u && delta.frame.pacerInvariantViolations == 0u, "every present had its credit; every pending move a wake");
    Require(MostFrameOwingDispatchesPerPresent(capture.records) == 1u, "trace: at most one frame-owing pointer-move dispatch per present");
    size_t dispatched = 0u;
    for (const Trace::Record& record : capture.records)
    {
        if (record.event != Trace::Event::InputDispatched)
            continue;
        ++dispatched;
        Require(record.detail == static_cast<uint8_t>(Trace::InputKind::PointerMove) && record.moves == static_cast<uint32_t>(kMoves) && record.owesFrame,
                "a coalesced dispatch stands for every move it absorbed");
        Require(record.input == record.firstInput + static_cast<uint64_t>(kMoves - 1) && record.firstArrivalQpc != 0u &&
                    record.firstArrivalQpc <= record.lastArrivalQpc && record.lastArrivalQpc <= record.qpcTimestamp,
                "it names the first and the latest move it absorbed, and when they arrived");
    }
    Require(dispatched == static_cast<size_t>(kRounds), "one InputDispatched event per dispatch");
    paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
}

// The credit is held, but the previous move's frame has not rendered yet (WM_PAINT ranks below input): a second move renders
// that frame first, with the credit, and waits for the next one. A stream of moves before any WM_PAINT therefore still makes one
// dispatch per presented frame.
void TestPointerPacingSecondMoveRendersTheOwedFrameFirst()
{
    PacedProbeWindow paced;
    paced.TakeFrameLatencyObject();
    paced.ReleaseUnit(); // one credit is free
    const WorkCounterScope work;
    paced.Move(30, 30);
    Require(paced.probe->moves == 1u && work.Delta().frame.presents == 0u, "the first move is dispatched at once; its frame is owed");
    paced.Move(31, 30);
    WorkCounters delta = work.Delta();
    Require(delta.frame.pointerMoveOwedRenders == 1u && delta.frame.presents == 1u, "the second move rendered the owed frame first");
    Require(paced.probe->moves == 1u && paced.State().pointerMovePending && paced.State().waitArmed, "and waits for the next frame");
    for (int move = 0; move < 10; ++move)
        paced.Move(32 + move, 30);
    delta = work.Delta();
    Require(delta.frame.pointerMoveOwedRenders == 1u && delta.frame.presents == 1u && paced.probe->moves == 1u, "later moves merge into the pending one");
    paced.ReleaseUnit();
    PumpUntil([&] { return ! paced.State().pointerMovePending && work.Delta().frame.presents == 2u; });
    delta = work.Delta();
    Require(paced.probe->moves == 2u && SamePoint(paced.probe->lastMove, paced.Dip(41, 30)), "the next frame dispatched the latest move");
    Require(delta.frame.presentsWithoutCredit == 0u && delta.frame.pacerInvariantViolations == 0u, "with its credit");
    paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
}

// While something animates, the pending move is dispatched at the start of the paced frame, before Tick: the tick reads the
// latest position.
void TestPointerPacingPendingMoveIsDispatchedBeforeTick()
{
    PacedProbeWindow paced;
    paced.TakeFrameLatencyObject();
    paced.Host().RequestAnimation(*paced.probe);
    PumpUntil([&] { return paced.State().continuous && paced.State().waitArmed; });
    for (int round = 0; round < 3; ++round)
    {
        paced.Move(50 + round, 40);
        paced.Move(60 + round, 40);
        Require(paced.State().pointerMovePending, "the moves wait for the paced frame");
        const size_t first = paced.probe->log.size();
        paced.ReleaseUnit();
        PumpUntil([&]
        {
            return std::any_of(paced.probe->log.begin() + static_cast<std::ptrdiff_t>(first), paced.probe->log.end(), [](const MoveProbe::Entry& entry) {
                return entry.kind == MoveProbe::Kind::Tick;
            });
        });
        const auto tick = std::find_if(paced.probe->log.begin() + static_cast<std::ptrdiff_t>(first),
                                       paced.probe->log.end(),
                                       [](const MoveProbe::Entry& entry) { return entry.kind == MoveProbe::Kind::Tick; });
        const auto move = std::find_if(paced.probe->log.begin() + static_cast<std::ptrdiff_t>(first),
                                       paced.probe->log.end(),
                                       [](const MoveProbe::Entry& entry) { return entry.kind == MoveProbe::Kind::Move; });
        Require(move != paced.probe->log.end() && move < tick, "the move is dispatched before the frame's Tick");
        Require(SamePoint(tick->lastMove, paced.Dip(60 + round, 40)), "the Tick reads the latest position");
    }
    paced.Host().CancelAnimation(*paced.probe);
    paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
}

// Every message that changes input, capture, focus, activation, mode or visibility dispatches a pending move first, so its
// handler (and every later one) sees the latest position and the order of events is kept.
void TestPointerPacingEveryFlushingMessageSeesTheLatestPosition()
{
    PacedProbeWindow paced;
    paced.TakeFrameLatencyObject();
    const HWND hwnd = paced.Hwnd();
    struct Message
    {
        const char* name;
        UINT message;
        WPARAM wParam;
        bool screenPoint; // lParam is a screen point (else the client point, or as given)
        LPARAM lParam;
    };
    const std::array<Message, 30u> messages{{
        {"WM_LBUTTONDOWN", WM_LBUTTONDOWN, MK_LBUTTON, false, 0},
        {"WM_LBUTTONUP", WM_LBUTTONUP, 0u, false, 0},
        {"WM_LBUTTONDBLCLK", WM_LBUTTONDBLCLK, MK_LBUTTON, false, 0},
        {"WM_LBUTTONUP", WM_LBUTTONUP, 0u, false, 0},
        {"WM_RBUTTONDOWN", WM_RBUTTONDOWN, MK_RBUTTON, false, 0},
        {"WM_RBUTTONUP", WM_RBUTTONUP, 0u, false, 0},
        {"WM_MBUTTONDOWN", WM_MBUTTONDOWN, MK_MBUTTON, false, 0},
        {"WM_MBUTTONUP", WM_MBUTTONUP, 0u, false, 0},
        {"WM_XBUTTONDOWN", WM_XBUTTONDOWN, MAKEWPARAM(MK_XBUTTON1, XBUTTON1), false, 0},
        {"WM_XBUTTONUP", WM_XBUTTONUP, MAKEWPARAM(0u, XBUTTON1), false, 0},
        {"WM_MOUSEWHEEL", WM_MOUSEWHEEL, MAKEWPARAM(0u, static_cast<WORD>(-WHEEL_DELTA)), true, 0},
        {"WM_MOUSEHWHEEL", WM_MOUSEHWHEEL, MAKEWPARAM(0u, static_cast<WORD>(WHEEL_DELTA)), true, 0},
        {"WM_MOUSEHOVER", WM_MOUSEHOVER, 0u, false, 0},
        {"WM_MOUSELEAVE", WM_MOUSELEAVE, 0u, false, -1},
        {"WM_NCMOUSEMOVE", WM_NCMOUSEMOVE, HTNOWHERE, true, 0},
        {"WM_NCMOUSELEAVE", WM_NCMOUSELEAVE, 0u, false, -1},
        {"WM_CONTEXTMENU", WM_CONTEXTMENU, 0u, false, -1},
        {"WM_CAPTURECHANGED", WM_CAPTURECHANGED, 0u, false, -1},
        {"WM_CANCELMODE", WM_CANCELMODE, 0u, false, -1},
        {"WM_KEYDOWN", WM_KEYDOWN, VK_F24, false, 1},
        {"WM_KEYUP", WM_KEYUP, VK_F24, false, static_cast<LPARAM>(0xC0000001u)},
        {"WM_CHAR", WM_CHAR, L'x', false, 1},
        {"WM_SETFOCUS", WM_SETFOCUS, 0u, false, -1},
        {"WM_KILLFOCUS", WM_KILLFOCUS, 0u, false, -1},
        {"WM_ACTIVATE", WM_ACTIVATE, WA_INACTIVE, false, -1},
        {"WM_ACTIVATEAPP", WM_ACTIVATEAPP, FALSE, false, -1},
        {"WM_ENTERMENULOOP", WM_ENTERMENULOOP, FALSE, false, -1},
        {"WM_ENTERSIZEMOVE", WM_ENTERSIZEMOVE, 0u, false, -1},
        {"WM_SHOWWINDOW", WM_SHOWWINDOW, FALSE, false, -1},
        {"WM_SIZE", WM_SIZE, SIZE_MINIMIZED, false, -2},
    }};
    RECT client{};
    Require(GetClientRect(hwnd, &client) != FALSE, "the client area");
    int x = 20;
    for (const Message& message : messages)
    {
        // A move that waits (the wait for the next frame is armed), then the message, at the moved point.
        ++x;
        paced.Move(x, 25);
        Require(paced.State().pointerMovePending, message.name);
        const size_t moves  = paced.probe->moves;
        const size_t first  = paced.probe->log.size();
        const LPARAM lParam = message.screenPoint    ? paced.Screen(x, 25)
                              : message.lParam == -1 ? 0
                              : message.lParam == -2 ? MAKELPARAM(client.right, client.bottom)
                              : message.lParam != 0  ? message.lParam
                                                     : MAKELPARAM(x, 25);
        static_cast<void>(SendMessageW(hwnd, message.message, message.wParam, lParam));
        if (paced.State().pointerMovePending || paced.probe->moves != moves + 1u || ! SamePoint(paced.probe->lastMove, paced.Dip(x, 25)))
            std::cerr << "    [W3.7] " << message.name << " did not dispatch the pending move first\n";
        Require(! paced.State().pointerMovePending && paced.probe->moves == moves + 1u && SamePoint(paced.probe->lastMove, paced.Dip(x, 25)),
                "the message dispatched the pending move");
        Require(paced.probe->log.size() > first && paced.probe->log[first].kind == MoveProbe::Kind::Move, "before anything the message itself did");
        for (size_t index = first + 1u; index < paced.probe->log.size(); ++index)
            Require(SamePoint(paced.probe->log[index].lastMove, paced.Dip(x, 25)), "every handler after it saw the latest position");
    }
    Require(paced.Host().DebugGetFramePacerStateForTest().paced, "the host is still paced");
    paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
}

// The ordering clause of the gate, alone: a button down after a burst of coalesced moves sees the latest position, and the
// burst costs one dispatch.
void TestPointerPacingButtonDownSeesTheLatestPosition()
{
    PacedProbeWindow paced;
    paced.TakeFrameLatencyObject();
    const WorkCounterScope work;
    for (int move = 0; move < 20; ++move)
        paced.Move(70 + move, 50);
    Require(paced.probe->moves == 0u && paced.State().pointerMovePending, "the moves wait");
    static_cast<void>(SendMessageW(paced.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(89, 50)));
    static_cast<void>(SendMessageW(paced.Hwnd(), WM_LBUTTONUP, 0u, MAKELPARAM(89, 50)));
    Require(paced.probe->moves == 1u && work.Delta().frame.pointerMoveDispatches == 1u, "one dispatch for the burst");
    const auto down =
        std::find_if(paced.probe->log.begin(), paced.probe->log.end(), [](const MoveProbe::Entry& entry) { return entry.kind == MoveProbe::Kind::Down; });
    Require(down != paced.probe->log.end() && SamePoint(down->lastMove, paced.Dip(89, 50)) && SamePoint(down->point, paced.Dip(89, 50)),
            "the button down sees the latest position");
    Require(paced.probe->log.front().kind == MoveProbe::Kind::Move, "the move reached the probe before the button down");
    paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
}

// WM_SETCURSOR is not a flush: Windows sends it before every uncaptured move, so flushing there would end coalescing. Its
// cursor query is answered at its own point, the latest one, with a fresh hit test: after a coalesced sequence the cursor is
// the latest point's, while the hover state may lag it by less than a frame.
void TestPointerPacingCursorAnswersTheLatestPoint()
{
    PacedProbeWindow paced;
    paced.TakeFrameLatencyObject();
    const HWND hwnd            = paced.Hwnd();
    static const HCURSOR hand  = LoadCursorW(nullptr, IDC_HAND);
    static const HCURSOR ibeam = LoadCursorW(nullptr, IDC_IBEAM);
    const auto query           = [&](int x, int y)
    {
        // As Windows delivers a move: WM_NCHITTEST with the screen point, then the cursor query, then the move.
        static_cast<void>(SendMessageW(hwnd, WM_NCHITTEST, 0u, paced.Screen(x, y)));
        static_cast<void>(SendMessageW(hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE)));
        paced.Move(x, y);
    };
    query(20, 20);  // a hand zone
    query(150, 20); // an I-beam zone
    Require(paced.State().pointerMovePending && paced.probe->moves == 0u, "the moves coalesced: the cursor queries did not flush them");
    Require(GetCursor() == ibeam, "the cursor is the latest point's");
    query(30, 20);
    Require(GetCursor() == hand && paced.State().pointerMovePending, "and follows the latest point again");
    paced.ReleaseUnit();
    PumpUntil([&] { return ! paced.State().pointerMovePending; });
    Require(paced.probe->moves == 1u && SamePoint(paced.probe->lastMove, paced.Dip(30, 20)), "the hover catches up at the next frame");
    paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
}

// Bounds: an unpaced host (no frame-latency object) and a hidden one dispatch at once; under a stalled compositor (no frame
// retires) a pending move is dispatched at the pacer's first wait timeout, without a present.
void TestPointerPacingUnpacedHiddenAndStalledHostsBoundTheWait()
{
    {
        PacedProbeWindow paced;
        paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
        Require(! paced.State().paced, "the chain is unpaced");
        const WorkCounterScope work;
        paced.Move(12, 12);
        paced.Move(13, 12);
        Require(paced.probe->moves == 2u && work.Delta().frame.pointerMovesCoalesced == 0u, "an unpaced host dispatches every move at once");
    }
    {
        PacedProbeWindow paced;
        paced.TakeFrameLatencyObject();
        ShowWindow(paced.Hwnd(), SW_HIDE);
        const WorkCounterScope work;
        paced.Move(12, 12);
        Require(paced.probe->moves == 1u && work.Delta().frame.pointerMovesCoalesced == 0u && ! paced.State().pointerMovePending,
                "a hidden host has no frame to wait for: it dispatches at once");
        paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
    }
    {
        PacedProbeWindow paced;
        constexpr DWORD kTimeoutMs = 60u;
        paced.TakeFrameLatencyObject(kTimeoutMs);
        const WorkCounterScope work;
        paced.Move(14, 12);
        Require(paced.State().pointerMovePending, "the move waits");
        PumpUntil([&] { return ! paced.State().pointerMovePending; });
        const WorkCounters delta = work.Delta();
        Require(paced.probe->moves == 1u && delta.frame.pacerWaitTimeouts == 1u, "a stalled compositor: the move is dispatched at the first wait timeout");
        Require(delta.frame.presents == 0u && delta.frame.presentsWithoutCredit == 0u, "and nothing is presented into a full queue");
        Require(delta.frame.pacerInvariantViolations == 0u, "the pending move always had a wake");
        paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
    }
}

// A move delivered while one of the host's own handlers runs (a control callback that sends or pumps, as a modal loop does) or
// while the host renders never renders the owed frame from there: it waits with a wake and is dispatched afterwards, not lost.
void TestPointerPacingNestedDeliveryNeverRendersReentrantly()
{
    {
        PacedProbeWindow paced;
        paced.TakeFrameLatencyObject();
        paced.ReleaseUnit();
        paced.Move(30, 30); // dispatched at once with the credit; its frame is owed
        Require(paced.probe->moves == 1u && GetUpdateRect(paced.Hwnd(), nullptr, FALSE) != FALSE, "a frame is owed");
        const WorkCounterScope work;
        bool pendingInside   = false;
        size_t rendersInside = 0u;
        paced.probe->onDown  = [&](DxUi::ControlHost&)
        {
            const WorkCounterScope inside;
            paced.Move(40, 30);
            paced.Move(41, 30);
            pendingInside = paced.State().pointerMovePending;
            rendersInside = static_cast<size_t>(inside.Delta().frame.renders);
        };
        static_cast<void>(SendMessageW(paced.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(30, 30)));
        paced.probe->onDown = nullptr;
        Require(pendingInside && rendersInside == 0u && work.Delta().frame.pointerMoveOwedRenders == 0u,
                "inside a control callback the moves wait: no nested render");
        PumpUntil([&] { return ! paced.State().pointerMovePending; });
        Require(paced.probe->moves == 2u && SamePoint(paced.probe->lastMove, paced.Dip(41, 30)), "the posted flush dispatched them afterwards");
        Require(work.Delta().frame.pacerInvariantViolations == 0u && work.Delta().frame.pointerMoveOwedRenders == 0u, "always with a wake");
        static_cast<void>(SendMessageW(paced.Hwnd(), WM_LBUTTONUP, 0u, MAKELPARAM(41, 30)));
        paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
    }
    {
        PacedProbeWindow paced;
        Require(AwaitFrameCreditForTest(paced.Host()), "the host holds its credit");
        bool sent            = false;
        paced.probe->onPaint = [&]
        {
            if (std::exchange(sent, true))
                return;
            paced.Move(55, 33);
        };
        const WorkCounterScope work;
        paced.probe->Repaint(paced.Host());
        static_cast<void>(UpdateWindow(paced.Hwnd()));
        paced.probe->onPaint = nullptr;
        Require(sent && paced.probe->nestedPaints == 0u && work.Delta().frame.pointerMoveOwedRenders == 0u, "a move sent from a paint renders nothing nested");
        Require(paced.probe->moves == 0u && work.Delta().frame.pointerMovesCoalesced == 1u, "it waited instead of being dispatched into the render");
        PumpUntil([&] { return ! paced.State().pointerMovePending && paced.probe->moves == 1u; });
        Require(SamePoint(paced.probe->lastMove, paced.Dip(55, 33)) && work.Delta().frame.pacerInvariantViolations == 0u, "and was dispatched afterwards");
    }
}

// A pending move whose wait goes with its swap chain (a device loss here) is dispatched from a posted flush. The flush carries
// an identity: one still queued when the host detaches (and re-attaches to the same window) or when the window is destroyed
// dispatches nothing.
void TestPointerPacingReleasedChainFlushesByIdentity()
{
    {
        PacedProbeWindow paced;
        paced.TakeFrameLatencyObject();
        const WorkCounterScope work;
        paced.Move(22, 22);
        Require(paced.State().pointerMovePending, "the move waits");
        paced.Host().DebugSimulateDeviceLoss();
        PumpUntil([&] { return ! paced.State().pointerMovePending; });
        Require(paced.probe->moves == 1u && SamePoint(paced.probe->lastMove, paced.Dip(22, 22)), "the posted flush dispatched the move");
        Require(work.Delta().frame.pacerInvariantViolations == 0u && work.Delta().frame.pointerFlushesStale == 0u, "it always had a wake");
    }
    {
        PacedProbeWindow paced;
        paced.TakeFrameLatencyObject();
        paced.Move(23, 22);
        paced.Host().DebugSimulateDeviceLoss();
        Require(paced.State().pointerMovePending && ! paced.State().waitArmed, "a flush is posted");
        const WorkCounterScope work;
        const HWND hwnd = paced.Hwnd();
        paced.Host().Detach();
        Require(paced.Host().Attach(hwnd, DxUi::WindowHost::AttachOptions{.forceWarp = true}), "re-attach to the same window");
        PumpUntil([&] { return work.Delta().frame.pointerFlushesStale >= 1u; });
        Require(work.Delta().frame.pointerMoveDispatches == 0u, "the earlier attachment's flush dispatches nothing");
    }
    {
        const WorkCounterScope work;
        {
            PacedProbeWindow paced;
            paced.TakeFrameLatencyObject();
            paced.Move(24, 22);
            paced.Host().DebugSimulateDeviceLoss();
            Require(paced.State().pointerMovePending, "a flush is posted");
        } // detached and destroyed with the flush still queued
        PumpOnce();
        Require(work.Delta().frame.pointerMoveDispatches == 0u, "a destroyed window's flush dispatches nothing");
    }
}

// Keyboard input is never coalesced: every key reaches its control while a frame is in flight.
void TestPointerPacingKeyboardIsNotCoalesced()
{
    PacedProbeWindow paced;
    paced.TakeFrameLatencyObject();
    paced.probe->SetFocusable(true);
    paced.Host().SetFocusControl(paced.probe, false);
    for (int key = 0; key < 5; ++key)
    {
        static_cast<void>(SendMessageW(paced.Hwnd(), WM_KEYDOWN, VK_F24, 1));
        static_cast<void>(SendMessageW(paced.Hwnd(), WM_KEYUP, VK_F24, static_cast<LPARAM>(0xC0000001u)));
    }
    Require(paced.probe->keys == 5u, "every key reached the control at once");
    paced.Host().DebugSetFramePacerWaitableForTest(nullptr);
}

// Hardware only. FX3's grid (GridScrollScenario, the native form W0.8's gate used) on the default hardware adapter, with a
// thread posting a pointer move every millisecond along the grid's rows for 5 s while the UI thread pumps. For each dispatch that
// left a frame owed, input to present is measured from the FIRST move it absorbed (so the wait for a frame in flight is
// counted) and, as evidence, from the latest; the present is the first one whose input number reaches the dispatch's latest
// move. Gate (read by the conductor in a hardware slot at 60 and at 144 Hz, DXUI_W37_HARDWARE_GATE=1): p99 from the first move
// <= one refresh interval, and at most one frame-owing dispatch per present. Writes .build/w37/input-to-present-<hz>hz.json.
void TestPointerPacingInputToPresentOnHardware()
{
    wil::com_ptr<IDXGIFactory1> factory;
    wil::com_ptr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 description{};
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.put()))) || FAILED(factory->EnumAdapters1(0u, adapter.put())) ||
        FAILED(adapter->GetDesc1(&description)) || (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u)
    {
        std::cout << "    [CAPABILITY SKIP] W3.7 input to present: no hardware adapter\n";
        return;
    }
    GridScrollScenario scenario;
    const HWND hwnd = scenario.Window().Hwnd();
    if (! scenario.Window().Host().DebugGetFramePacerStateForTest().paced)
    {
        std::cout << "    [CAPABILITY SKIP] W3.7 input to present: the hardware chain offers no frame-latency waitable object\n";
        return;
    }
    DWM_TIMING_INFO timing{};
    timing.cbSize = sizeof(timing);
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    const uint64_t periodQpc = SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &timing)) ? static_cast<uint64_t>(timing.qpcRefreshPeriod) : 0u;
    const uint64_t hz        = periodQpc != 0u ? (static_cast<uint64_t>(frequency.QuadPart) + periodQpc / 2u) / periodQpc : 0u;
    RECT client{};
    GetClientRect(hwnd, &client);
    RecordCapture capture(1u << 18u);
    std::atomic<bool> running{true};
    {
        const ScopedSink sink(capture);
        std::thread mover([&]
        {
            wil::unique_handle timer(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS));
            for (int step = 0; running.load(std::memory_order_relaxed); ++step)
            {
                const int y = 30 + (step % std::max(1, static_cast<int>(client.bottom) - 40));
                static_cast<void>(PostMessageW(hwnd, WM_MOUSEMOVE, 0u, MAKELPARAM(client.right / 2, y)));
                LARGE_INTEGER due{};
                due.QuadPart = -10'000; // 1 ms
                if (timer && SetWaitableTimer(timer.get(), &due, 0, nullptr, nullptr, FALSE) != FALSE)
                    static_cast<void>(WaitForSingleObject(timer.get(), INFINITE));
                else
                    Sleep(1u);
            }
        });
        const ULONGLONG end = GetTickCount64() + 5'000u;
        while (GetTickCount64() < end)
        {
            PumpOnce();
            static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, 5u, QS_ALLINPUT, MWMO_INPUTAVAILABLE));
        }
        running.store(false, std::memory_order_relaxed);
        mover.join();
        scenario.Window().PumpMessages();
    }
    std::vector<const Trace::Record*> presents;
    for (const Trace::Record& record : capture.records)
        if (record.event == Trace::Event::Present && SUCCEEDED(record.result))
            presents.push_back(&record);
    std::vector<double> fromFirstUs;
    std::vector<double> fromLastUs;
    size_t dispatches = 0u;
    size_t moves      = 0u;
    for (const Trace::Record& record : capture.records)
    {
        if (record.event != Trace::Event::InputDispatched || ! record.owesFrame || record.firstArrivalQpc == 0u)
            continue;
        ++dispatches;
        moves += record.moves;
        const auto shown = std::find_if(presents.begin(), presents.end(), [&](const Trace::Record* present) { return present->input >= record.input; });
        if (shown == presents.end())
            continue;
        const double perUs = 1'000'000.0 / static_cast<double>(frequency.QuadPart);
        fromFirstUs.push_back(static_cast<double>((*shown)->qpcTimestamp - record.firstArrivalQpc) * perUs);
        fromLastUs.push_back(static_cast<double>((*shown)->qpcTimestamp - record.lastArrivalQpc) * perUs);
    }
    const auto percentile = [](std::vector<double> values, double share) noexcept
    {
        if (values.empty())
            return 0.0;
        std::ranges::sort(values);
        return values[std::min(values.size() - 1u, static_cast<size_t>(share * static_cast<double>(values.size())))];
    };
    const double periodUs  = periodQpc != 0u ? static_cast<double>(periodQpc) * 1'000'000.0 / static_cast<double>(frequency.QuadPart) : 0.0;
    const double p50First  = percentile(fromFirstUs, 0.5);
    const double p99First  = percentile(fromFirstUs, 0.99);
    const double p50Last   = percentile(fromLastUs, 0.5);
    const double p99Last   = percentile(fromLastUs, 0.99);
    const size_t mostPer   = MostFrameOwingDispatchesPerPresent(capture.records);
    const size_t presented = presents.size();
    const size_t overflow  = capture.overflow;
    std::filesystem::create_directories(".build/w37");
    const std::string path = ".build/w37/input-to-present-" + std::to_string(hz) + "hz.json";
    std::ofstream output(path, std::ios::binary);
    output << "{\"item\":\"W3.7\",\"fixture\":\"FX3\",\"host\":\"WindowHost\",\"adapter\":\"hardware\",\"refreshHz\":" << hz;
    output << ",\"refreshPeriodUs\":" << periodUs << ",\"frameOwingDispatches\":" << dispatches << ",\"movesAbsorbed\":" << moves;
    output << ",\"presents\":" << presented << ",\"mostFrameOwingDispatchesPerPresent\":" << mostPer;
    output << ",\"fromFirstMoveUs\":{\"p50\":" << p50First << ",\"p99\":" << p99First << "}";
    output << ",\"fromLatestMoveUs\":{\"p50\":" << p50Last << ",\"p99\":" << p99Last << "}";
    output << ",\"overflow\":" << overflow << "}\n";
    std::cout << "    [W3.7] hardware FX3 at " << hz << " Hz: " << dispatches << " frame-owing dispatches for " << moves << " moves, " << presented
              << " presents; input to present p99 " << p99First << " us from the first move, " << p99Last << " us from the latest (refresh " << periodUs
              << " us); at most " << mostPer << " dispatch per present (" << path << ")\n";
    Require(dispatches >= 50u && capture.overflow == 0u, "the stream of moves reached the grid and was traced");
    Require(mostPer <= 1u, "at most one frame-owing pointer-move dispatch per present");
    std::array<wchar_t, 8u> gate{};
    if (GetEnvironmentVariableW(L"DXUI_W37_HARDWARE_GATE", gate.data(), static_cast<DWORD>(gate.size())) == 0u || gate[0] != L'1')
    {
        std::cout << "    [EVIDENCE ONLY] W3.7 hardware gate not judged: set DXUI_W37_HARDWARE_GATE=1 in the hardware slot\n";
        return;
    }
    Require(periodUs > 0.0 && p99First <= periodUs, "W3.7 gate (G8): input to present within one refresh interval at p99, from the first move absorbed");
}
