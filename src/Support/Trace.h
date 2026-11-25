#pragma once

// The DxUi trace provider (Specs/Core/Core_PerformanceAndResources.md, "Trace provider"; schema in docs/performance.md).
// One TraceLogging provider, registered for the life of the process and dormant unless an ETW session enables it or a test
// installs a sink. Every event site is one relaxed load and one predictable branch:
//
//     if (Detail::Trace::Listening()) [[unlikely]]
//         Detail::Trace::...;
//
// Nothing is allocated, formatted or locked on either side of that branch: an event is a fixed record of identifiers,
// counts, sizes, reasons, results and one QueryPerformanceCounter timestamp, written on the thread doing the work. Events
// never carry text, control names, paths or document content. Each event site sits beside the work counter
// (Support/WorkCounters.h) of the same moment, where one exists.

#include <atomic>
#include <cstdint>
#include <windows.h>

namespace DxUi::Detail::Trace
{
// The provider: name "DxUi", GUID {89b12dac-c17b-5934-baa0-35b9f94990e6} (the EventSource name hash of "DxUi").
inline constexpr GUID kProviderId{0x89b12dacu, 0xc17bu, 0x5934u, {0xbau, 0xa0u, 0x35u, 0xb9u, 0xf9u, 0x49u, 0x90u, 0xe6u}};
inline constexpr char kProviderName[] = "DxUi";

// Keywords a session can select; every event is written at level 4 (information).
inline constexpr uint64_t kKeywordInput = 0x1u;
inline constexpr uint64_t kKeywordFrame = 0x2u;
inline constexpr uint64_t kKeywordWake  = 0x4u;

enum class Event : uint8_t
{
    InputReceived   = 1u,
    FrameRequested  = 2u,
    PrepareBegin    = 3u,
    PrepareEnd      = 4u,
    Present         = 5u,
    Composite       = 6u,
    Wake            = 7u,
    HitReuseOff     = 8u,  // A host stops reusing pointer hit results: an unreported control attached (W2.9).
    FramePaced      = 9u,  // A native paced frame's animation sample against the compositor's target time (W3.6).
    InputDispatched = 10u, // A native host dispatched a pointer move to its controls, alone or for the moves it absorbed (W3.7).
};

enum class InputKind : uint8_t
{
    PointerMove   = 1u,
    PointerDown   = 2u,
    PointerUp     = 3u,
    Wheel         = 4u,
    KeyDown       = 5u,
    KeyUp         = 6u,
    Character     = 7u,
    PointerLeave  = 8u, // Embedded: a dispatched pointer leave.
    PointerCancel = 9u, // Embedded: a dispatched pointer cancel.
};

// Why a host asked for a frame: a native WindowHost invalidates its window, an EmbeddedHost calls the application's
// requestPreparation.
enum class FrameReason : uint8_t
{
    Invalidate   = 1u, // A control or the host changed what the view shows.
    Visibility   = 2u, // The view became visible again.
    Recovery     = 3u, // A device or resource recovery retry.
    Continuation = 4u, // Cooperative content preparation continues, or the view changed during its own preparation.
    InputWake    = 5u, // Embedded: the wake an input dispatch deferred until it returned.
};

enum class WakeSource : uint8_t
{
    AnimationTimer  = 1u,
    EmbeddedAdvance = 2u,
    MenuHoverTimer  = 3u,
    PaintMessage    = 4u,
    PacedFrame      = 5u, // A native host's paced-frame message (W3.6).
    PacerTimer      = 6u, // A native host's skip timer: a paced tick that presented nothing ticks again a period later.
};

// FramePaced: where the animation sample came from.
enum class PacedSampleSource : uint8_t
{
    Compositor = 1u, // DCompositionGetStatistics: the first target time after now.
    Fallback   = 2u, // Statistics unavailable: the current time.
    TestClock  = 3u, // A test's animation clock.
};

// One event as the provider writes it and a test sink receives it. Fields an event does not use are 0.
struct Record final
{
    Event event           = Event::InputReceived;
    uint8_t detail        = 0u;    // InputKind, FrameReason or WakeSource.
    bool partial          = false; // Present: a partial (dirty-rectangle) present.
    uint32_t hostId       = 0u;    // Process-unique host number, 1-based; 0 when the work belongs to no single host.
    uint32_t threadId     = 0u;
    int32_t result        = 0;     // PrepareEnd, Present: the HRESULT.
    uint64_t frame        = 0u;    // The host's frame sequence (1-based): the frame requested, prepared or presented.
    uint64_t input        = 0u;    // The host's input sequence (1-based): the input received, or the newest one a frame includes.
    uint64_t pixels       = 0u;    // PrepareEnd: device pixels cleared and repainted.
    uint64_t qpcTimestamp = 0u;    // QueryPerformanceCounter ticks, the one clock of every event.
    uint64_t targetQpc    = 0u;    // FramePaced: the compositor target time the frame's present is predicted for (0 on fallback).
    uint64_t sampleQpc    = 0u;    // FramePaced: the time the animation sample stands for; targetQpc unless clamped or fallen back.
    uint64_t periodQpc    = 0u;    // FramePaced: the compositor's refresh period (0 when unknown).
    bool clamped          = false; // FramePaced: the hitch clamp shortened this frame's step.
    // InputDispatched (W3.7): `input` is the latest move dispatched; these name the first move it absorbed, when the first and the
    // latest arrived (QueryPerformanceCounter ticks), how many moves it stands for and whether it left a frame owed.
    uint64_t firstInput      = 0u;
    uint64_t firstArrivalQpc = 0u;
    uint64_t lastArrivalQpc  = 0u;
    uint32_t moves           = 0u;
    bool owesFrame           = false;
};

// A test-only observer of every record, on the thread that writes it.
using Sink = void (*)(void* context, const Record& record) noexcept;

inline constexpr uint32_t kSessionListening = 0x1u;
inline constexpr uint32_t kSinkInstalled    = 0x2u;
// Who listens: an ETW session (set by the provider's enable callback) and/or a test sink.
inline std::atomic<uint32_t> g_listeners{0u};

[[nodiscard]] inline bool Listening() noexcept
{
    return g_listeners.load(std::memory_order_relaxed) != 0u;
}

// Writes one record: the TraceLogging event while a session listens, and the sink's copy while one is installed. Stamps the
// thread and, when the record has none, the timestamp.
void Write(Record record) noexcept;
// A process-unique host number, assigned on a host's first traced event.
[[nodiscard]] uint32_t NextHostId() noexcept;
// Test-only: installs (or, with nullptr, removes) the one process-wide sink. Not reentrant with tracing on other threads.
void InstallSinkForTest(Sink sink, void* context) noexcept;
// True while an ETW session has the provider enabled.
[[nodiscard]] inline bool SessionListening() noexcept
{
    return (g_listeners.load(std::memory_order_relaxed) & kSessionListening) != 0u;
}

// Per-host correlation, kept by the host (ControlHost::TraceCorrelation: hostId, frameRequested, inputSequence,
// frameSequence, frameInputSequence) and touched only on the traced side of an event site. An input-received event carries
// the host's input number; the frame a host prepares next records the newest input received before its preparation began;
// its present (or composite) carries that number. Input-to-present for input N is the first present on the same host
// whose input number is at least N.
template <typename Correlation> [[nodiscard]] uint32_t HostIdOf(Correlation& correlation) noexcept
{
    if (correlation.hostId == 0u)
        correlation.hostId = NextHostId();
    return correlation.hostId;
}

template <typename Correlation> void InputReceived(Correlation& correlation, InputKind kind) noexcept
{
    Write(Record{.event = Event::InputReceived, .detail = uint8_t(kind), .hostId = HostIdOf(correlation), .input = ++correlation.inputSequence});
}

// The first request after a preparation begins writes one event naming the frame that will satisfy it; later requests for
// the same frame are coalesced, as the window system and the application coalesce them.
template <typename Correlation> void FrameRequested(Correlation& correlation, FrameReason reason) noexcept
{
    if (correlation.frameRequested)
        return;
    correlation.frameRequested = true;
    Write(Record{.event  = Event::FrameRequested,
                 .detail = uint8_t(reason),
                 .hostId = HostIdOf(correlation),
                 .frame  = correlation.frameSequence + 1u,
                 .input  = correlation.inputSequence});
}

template <typename Correlation> void PrepareBegin(Correlation& correlation) noexcept
{
    correlation.frameRequested     = false;
    correlation.frameInputSequence = correlation.inputSequence;
    Write(Record{.event = Event::PrepareBegin, .hostId = HostIdOf(correlation), .frame = ++correlation.frameSequence, .input = correlation.frameInputSequence});
}

template <typename Correlation> void PrepareEnd(Correlation& correlation, HRESULT result, uint64_t pixels) noexcept
{
    Write(Record{.event  = Event::PrepareEnd,
                 .hostId = HostIdOf(correlation),
                 .result = result,
                 .frame  = correlation.frameSequence,
                 .input  = correlation.frameInputSequence,
                 .pixels = pixels});
}

// Event::Present for a native swap-chain present, Event::Composite for an embedded composition.
template <typename Correlation> void Presented(Correlation& correlation, Event event, bool partial, HRESULT result) noexcept
{
    Write(Record{.event   = event,
                 .partial = partial,
                 .hostId  = HostIdOf(correlation),
                 .result  = result,
                 .frame   = correlation.frameSequence,
                 .input   = correlation.frameInputSequence});
}
// A host went from reusing pointer hit results to walking for every hit test: the first control whose hit testing it cannot
// see attached (ControlHost::DebugGetFirstUnreportedHitControlType names it).
template <typename Correlation> void HitReuseOff(Correlation& correlation) noexcept
{
    Write(Record{.event = Event::HitReuseOff, .hostId = HostIdOf(correlation)});
}
// A native paced frame (W3.6): the frame it samples for is the host's next prepared frame.
template <typename Correlation>
void FramePaced(Correlation& correlation, PacedSampleSource source, uint64_t targetQpc, uint64_t sampleQpc, uint64_t periodQpc, bool clamped) noexcept
{
    Write(Record{.event     = Event::FramePaced,
                 .detail    = uint8_t(source),
                 .hostId    = HostIdOf(correlation),
                 .frame     = correlation.frameSequence + 1u,
                 .targetQpc = targetQpc,
                 .sampleQpc = sampleQpc,
                 .periodQpc = periodQpc,
                 .clamped   = clamped});
}
// A native pointer-move dispatch (W3.7): the moves it stands for, from the first absorbed to the latest. Input to present for a
// coalesced dispatch is judged from firstArrivalQpc, so the wait for a frame in flight is counted.
template <typename Correlation>
void InputDispatched(Correlation& correlation,
                     uint64_t firstInput,
                     uint64_t lastInput,
                     uint64_t firstArrivalQpc,
                     uint64_t lastArrivalQpc,
                     uint32_t moves,
                     bool owesFrame) noexcept
{
    Write(Record{.event           = Event::InputDispatched,
                 .detail          = uint8_t(InputKind::PointerMove),
                 .hostId          = HostIdOf(correlation),
                 .frame           = correlation.frameSequence + 1u,
                 .input           = lastInput,
                 .firstInput      = firstInput,
                 .firstArrivalQpc = firstArrivalQpc,
                 .lastArrivalQpc  = lastArrivalQpc,
                 .moves           = moves,
                 .owesFrame       = owesFrame});
}
inline void Wake(WakeSource source, uint32_t hostId = 0u) noexcept
{
    Write(Record{.event = Event::Wake, .detail = uint8_t(source), .hostId = hostId});
}
} // namespace DxUi::Detail::Trace
