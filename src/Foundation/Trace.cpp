// The DxUi trace provider (Support/Trace.h). Registration happens once, at process start, and adds no thread, timer or
// window: ETW calls the enable callback when a session enables or disables the provider, and the callback only updates the
// listener flag every event site reads.
#include "Support/Trace.h"

#include <TraceLoggingProvider.h>
#include <evntrace.h>

#pragma comment(lib, "advapi32.lib")

TRACELOGGING_DEFINE_PROVIDER(g_dxuiTraceProvider, "DxUi", (0x89b12dac, 0xc17b, 0x5934, 0xba, 0xa0, 0x35, 0xb9, 0xf9, 0x49, 0x90, 0xe6));

namespace DxUi::Detail::Trace
{
namespace
{
constexpr UCHAR kLevel = 4u; // WINEVENT_LEVEL_INFO

std::atomic<uint32_t> g_nextHostId{0u};
std::atomic<Sink> g_sink{nullptr};
std::atomic<void*> g_sinkContext{nullptr};

void NTAPI OnProviderEnableChanged(LPCGUID, ULONG, UCHAR, ULONGLONG, ULONGLONG, PEVENT_FILTER_DESCRIPTOR, PVOID) noexcept
{
    // TraceLogging has already recorded the combined level and keywords of every session when it calls back.
    if (TraceLoggingProviderEnabled(g_dxuiTraceProvider, 0u, 0u))
        g_listeners.fetch_or(kSessionListening, std::memory_order_relaxed);
    else
        g_listeners.fetch_and(~kSessionListening, std::memory_order_relaxed);
}

// Registers the provider for the life of the process. A failed registration leaves it dormant: nothing is written.
struct ProviderRegistration final
{
    ProviderRegistration() noexcept
    {
        registered = SUCCEEDED(TraceLoggingRegisterEx(g_dxuiTraceProvider, &OnProviderEnableChanged, nullptr));
    }
    ~ProviderRegistration()
    {
        if (! registered)
            return;
        g_listeners.fetch_and(~kSessionListening, std::memory_order_relaxed);
        TraceLoggingUnregister(g_dxuiTraceProvider);
    }
    ProviderRegistration(const ProviderRegistration&)            = delete;
    ProviderRegistration& operator=(const ProviderRegistration&) = delete;
    ProviderRegistration(ProviderRegistration&&)                 = delete;
    ProviderRegistration& operator=(ProviderRegistration&&)      = delete;
    bool registered                                              = false;
};
const ProviderRegistration g_registration;

void WriteEvent(const Record& record) noexcept
{
    const UINT64 qpc = static_cast<UINT64>(record.qpcTimestamp);
    switch (record.event)
    {
        case Event::InputReceived:
            TraceLoggingWrite(g_dxuiTraceProvider,
                              "InputReceived",
                              TraceLoggingLevel(kLevel),
                              TraceLoggingKeyword(kKeywordInput),
                              TraceLoggingUInt32(record.hostId, "HostId"),
                              TraceLoggingUInt64(record.input, "Input"),
                              TraceLoggingUInt8(record.detail, "Kind"),
                              TraceLoggingUInt64(qpc, "Qpc"));
            break;
        case Event::FrameRequested:
            TraceLoggingWrite(g_dxuiTraceProvider,
                              "FrameRequested",
                              TraceLoggingLevel(kLevel),
                              TraceLoggingKeyword(kKeywordFrame),
                              TraceLoggingUInt32(record.hostId, "HostId"),
                              TraceLoggingUInt64(record.frame, "Frame"),
                              TraceLoggingUInt64(record.input, "Input"),
                              TraceLoggingUInt8(record.detail, "Reason"),
                              TraceLoggingUInt64(qpc, "Qpc"));
            break;
        case Event::PrepareBegin:
            TraceLoggingWrite(g_dxuiTraceProvider,
                              "PrepareBegin",
                              TraceLoggingLevel(kLevel),
                              TraceLoggingKeyword(kKeywordFrame),
                              TraceLoggingUInt32(record.hostId, "HostId"),
                              TraceLoggingUInt64(record.frame, "Frame"),
                              TraceLoggingUInt64(record.input, "Input"),
                              TraceLoggingUInt64(qpc, "Qpc"));
            break;
        case Event::PrepareEnd:
            TraceLoggingWrite(g_dxuiTraceProvider,
                              "PrepareEnd",
                              TraceLoggingLevel(kLevel),
                              TraceLoggingKeyword(kKeywordFrame),
                              TraceLoggingUInt32(record.hostId, "HostId"),
                              TraceLoggingUInt64(record.frame, "Frame"),
                              TraceLoggingUInt64(record.input, "Input"),
                              TraceLoggingHResult(record.result, "Result"),
                              TraceLoggingUInt64(record.pixels, "Pixels"),
                              TraceLoggingUInt64(qpc, "Qpc"));
            break;
        case Event::Present:
            TraceLoggingWrite(g_dxuiTraceProvider,
                              "Present",
                              TraceLoggingLevel(kLevel),
                              TraceLoggingKeyword(kKeywordFrame),
                              TraceLoggingUInt32(record.hostId, "HostId"),
                              TraceLoggingUInt64(record.frame, "Frame"),
                              TraceLoggingUInt64(record.input, "Input"),
                              TraceLoggingBoolean(record.partial ? TRUE : FALSE, "Partial"),
                              TraceLoggingHResult(record.result, "Result"),
                              TraceLoggingUInt64(qpc, "Qpc"));
            break;
        case Event::Composite:
            TraceLoggingWrite(g_dxuiTraceProvider,
                              "Composite",
                              TraceLoggingLevel(kLevel),
                              TraceLoggingKeyword(kKeywordFrame),
                              TraceLoggingUInt32(record.hostId, "HostId"),
                              TraceLoggingUInt64(record.frame, "Frame"),
                              TraceLoggingUInt64(record.input, "Input"),
                              TraceLoggingUInt64(qpc, "Qpc"));
            break;
        case Event::Wake:
            TraceLoggingWrite(g_dxuiTraceProvider,
                              "Wake",
                              TraceLoggingLevel(kLevel),
                              TraceLoggingKeyword(kKeywordWake),
                              TraceLoggingUInt32(record.hostId, "HostId"),
                              TraceLoggingUInt8(record.detail, "Source"),
                              TraceLoggingUInt64(qpc, "Qpc"));
            break;
        default: break;
    }
}
} // namespace

void Write(Record record) noexcept
{
    record.threadId = GetCurrentThreadId();
    if (record.qpcTimestamp == 0u)
    {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        record.qpcTimestamp = static_cast<uint64_t>(now.QuadPart);
    }
    if (TraceLoggingProviderEnabled(g_dxuiTraceProvider, kLevel, 0u))
        WriteEvent(record);
    if (const Sink sink = g_sink.load(std::memory_order_acquire))
        sink(g_sinkContext.load(std::memory_order_relaxed), record);
}

uint32_t NextHostId() noexcept
{
    return g_nextHostId.fetch_add(1u, std::memory_order_relaxed) + 1u;
}

void InstallSinkForTest(Sink sink, void* context) noexcept
{
    if (sink)
    {
        g_sinkContext.store(context, std::memory_order_relaxed);
        g_sink.store(sink, std::memory_order_release);
        g_listeners.fetch_or(kSinkInstalled, std::memory_order_relaxed);
        return;
    }
    g_listeners.fetch_and(~kSinkInstalled, std::memory_order_relaxed);
    g_sink.store(nullptr, std::memory_order_release);
    g_sinkContext.store(nullptr, std::memory_order_relaxed);
}
} // namespace DxUi::Detail::Trace
