#include "Controls.Tests.TraceProvider.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.GridScrollScenario.h"
#include "Support/Trace.h"
#include "Support/WorkCounters.h"

#include <algorithm>
#include <array>
#include <bcrypt.h>
#include <cstring>
#include <cwctype>
#include <evntcons.h>
#include <evntrace.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "bcrypt.lib")

using namespace DxUi;
namespace Trace = DxUi::Detail::Trace;

namespace
{
// Every record a test sink receives, in storage reserved before tracing starts: the sink itself never allocates.
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

// Installs a capture for one scope and always removes it.
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

// The EventSource-convention GUID of a provider name: SHA-1 over a fixed namespace and the upper-cased name in UTF-16BE,
// first 16 bytes, version 5. Tools that enable a provider by name ("*DxUi") derive the same GUID.
GUID ProviderGuidFromName(std::wstring_view name)
{
    std::vector<uint8_t> input{0x48u, 0x2Cu, 0x2Du, 0xB2u, 0xC3u, 0x90u, 0x47u, 0xC8u, 0x87u, 0xF8u, 0x1Au, 0x15u, 0xBFu, 0xC1u, 0x30u, 0xFBu};
    for (const wchar_t character : name)
    {
        const auto upper = static_cast<uint16_t>(towupper(character));
        input.push_back(static_cast<uint8_t>(upper >> 8u));
        input.push_back(static_cast<uint8_t>(upper & 0xFFu));
    }
    std::array<uint8_t, 20u> hash{};
    Require(BCRYPT_SUCCESS(BCryptHash(BCRYPT_SHA1_ALG_HANDLE, nullptr, 0u, input.data(), static_cast<ULONG>(input.size()), hash.data(), 20u)),
            "hash the provider name");
    hash[7] = static_cast<uint8_t>((hash[7] & 0x0Fu) | 0x50u);
    GUID guid{};
    std::memcpy(&guid, hash.data(), sizeof(guid));
    return guid;
}

[[nodiscard]] std::vector<Trace::Record> RecordsOf(const RecordCapture& capture, Trace::Event event)
{
    std::vector<Trace::Record> result;
    for (const Trace::Record& record : capture.records)
        if (record.event == event)
            result.push_back(record);
    return result;
}
} // namespace

void TestTraceProviderIsDormantUntilSomethingListens()
{
    Require((Trace::g_listeners.load() & Trace::kSinkInstalled) == 0u, "no test sink is installed outside a test that installs one");
    Require(IsEqualGUID(Trace::kProviderId, ProviderGuidFromName(L"DxUi")) != FALSE, "the provider GUID is the name hash of \"DxUi\"");
    Require(std::string_view(Trace::kProviderName) == "DxUi", "the provider is named DxUi");

    RecordCapture capture(64u);
    {
        const ScopedSink sink(capture);
        Require(Trace::Listening(), "an installed sink makes every event site take its traced branch");
        Trace::Wake(Trace::WakeSource::AnimationTimer);
    }
    Require(capture.records.size() == 1u, "the sink receives the one event written while it listened");
    const Trace::Record& wake = capture.records.front();
    Require(wake.event == Trace::Event::Wake && wake.detail == uint8_t(Trace::WakeSource::AnimationTimer) && wake.hostId == 0u,
            "a wake event carries its source");
    Require(wake.threadId == GetCurrentThreadId() && wake.qpcTimestamp != 0u, "an event is stamped with its thread and a QueryPerformanceCounter time");
    Require(Trace::Listening() == Trace::SessionListening(), "removing the sink leaves only an ETW session, if any, listening");

    // Nothing listening: a host's frame writes nothing and leaves its correlation untouched.
    GridScrollScenario scenario;
    scenario.Wheel();
    scenario.Paint();
    Require(capture.records.size() == 1u, "a removed sink receives nothing more");
    if (Trace::SessionListening())
        std::cerr << "    [TRACE] an ETW session has the DxUi provider enabled during this run\n";
}

void TestTraceProviderPairsEveryGridScrollInputWithItsPresent()
{
    constexpr size_t kWheelNotches = 100u;
    constexpr size_t kMoves        = 20u;
    constexpr size_t kKeys         = 10u;
    GridScrollScenario scenario;
    Require(scenario.Grid().GetVisibleRowRect(0u).has_value(), "the grid starts at its first row");

    RecordCapture capture(16384u);
    std::vector<Trace::InputKind> sent;
    sent.reserve(kWheelNotches + kMoves + 2u * kKeys);
    const Detail::WorkCounterScope work;
    {
        const ScopedSink sink(capture);
        for (size_t index = 0u; index < kWheelNotches; ++index)
        {
            scenario.Wheel();
            sent.push_back(Trace::InputKind::Wheel);
            scenario.Paint();
            if (index % (kWheelNotches / kMoves) == 0u)
            {
                scenario.Move(static_cast<int>(index % 7u), static_cast<int>(index % 5u));
                sent.push_back(Trace::InputKind::PointerMove);
                scenario.Paint();
            }
            if (index % (kWheelNotches / kKeys) == 0u)
            {
                scenario.Key(VK_DOWN);
                sent.push_back(Trace::InputKind::KeyDown);
                sent.push_back(Trace::InputKind::KeyUp);
                scenario.Paint();
            }
        }
    }
    const Detail::WorkCounters delta = work.Delta();
    Require(capture.overflow == 0u, "the capture holds every record");
    Require(! scenario.Grid().GetVisibleRowRect(0u).has_value(), "the wheel scrolled the grid past its first row");

    // Exactly one input-received event per input, in order, numbered consecutively on one host.
    const std::vector<Trace::Record> inputs = RecordsOf(capture, Trace::Event::InputReceived);
    Require(inputs.size() == sent.size(), "every input yields exactly one input-received event");
    const uint32_t hostId = inputs.empty() ? 0u : inputs.front().hostId;
    Require(hostId != 0u, "input events name their host");
    for (size_t index = 0u; index < inputs.size(); ++index)
    {
        Require(inputs[index].hostId == hostId, "every input belongs to the scenario's host");
        Require(inputs[index].detail == uint8_t(sent[index]), "each input event names the kind of input sent");
        Require(index == 0u || inputs[index].input == inputs[index - 1u].input + 1u, "input numbers are consecutive");
    }

    // Exactly one present event per present, each after the preparation of the same frame.
    const std::vector<Trace::Record> presents = RecordsOf(capture, Trace::Event::Present);
    Require(presents.size() == delta.frame.presents, "every present yields exactly one present event, matching the present counter");
    Require(RecordsOf(capture, Trace::Event::PrepareBegin).size() == delta.frame.renders, "every render pass yields one prepare-begin event");
    for (size_t index = 0u; index < capture.records.size(); ++index)
    {
        const Trace::Record& record = capture.records[index];
        if (record.event != Trace::Event::Present)
            continue;
        Require(record.hostId == hostId && SUCCEEDED(record.result), "the scenario's presents succeed");
        Require(index >= 2u && capture.records[index - 1u].event == Trace::Event::PrepareEnd &&
                    capture.records[index - 2u].event == Trace::Event::PrepareBegin && capture.records[index - 1u].frame == record.frame &&
                    capture.records[index - 2u].frame == record.frame,
                "a present follows the prepare begin and end of its own frame");
    }
    for (size_t index = 1u; index < presents.size(); ++index)
        Require(presents[index].frame > presents[index - 1u].frame && presents[index].input >= presents[index - 1u].input,
                "frame and input numbers never go back");

    // Input to present, per input: the first present on the host whose input number reaches the input's. A wheel notch is
    // painted before the next input, so its present carries exactly its number, after a frame request naming that frame.
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    std::vector<double> wheelLatencyUs;
    for (const Trace::Record& input : inputs)
    {
        const auto shown = std::ranges::find_if(presents, [&](const Trace::Record& present) { return present.input >= input.input; });
        if (input.detail != uint8_t(Trace::InputKind::Wheel))
            continue;
        Require(shown != presents.end(), "every wheel notch reaches a present");
        Require(shown->input == input.input && shown->qpcTimestamp >= input.qpcTimestamp, "a wheel notch's present carries its input number, after it");
        const bool requested = std::ranges::any_of(capture.records,
                                                   [&](const Trace::Record& record)
        {
            return record.event == Trace::Event::FrameRequested && record.frame == shown->frame && record.input == input.input &&
                   record.detail == uint8_t(Trace::FrameReason::Invalidate) && record.qpcTimestamp >= input.qpcTimestamp &&
                   record.qpcTimestamp <= shown->qpcTimestamp;
        });
        Require(requested, "the wheel notch's invalidation requests the frame that shows it");
        wheelLatencyUs.push_back(static_cast<double>(shown->qpcTimestamp - input.qpcTimestamp) * 1'000'000.0 / static_cast<double>(frequency.QuadPart));
    }
    Require(wheelLatencyUs.size() == kWheelNotches, "input to present is computed for every wheel notch");
    std::ranges::sort(wheelLatencyUs);
    std::cerr << "    [TRACE] inputs=" << inputs.size() << " presents=" << presents.size()
              << " wheel input-to-present us p50=" << wheelLatencyUs[wheelLatencyUs.size() / 2u] << " max=" << wheelLatencyUs.back()
              << " (WARP or hardware as the host chose; not a timing verdict)\n";
}

namespace
{
// One DxUi event read back from ETW: its name and the fields the sink also received.
struct EtwRecord final
{
    std::string name;
    uint32_t hostId   = 0u;
    uint64_t frame    = 0u;
    uint64_t input    = 0u;
    uint8_t detail    = 0u;
    uint64_t qpc      = 0u;
    uint32_t threadId = 0u;
};

struct EtwConsumer final
{
    EtwConsumer()                              = default;
    EtwConsumer(const EtwConsumer&)            = delete;
    EtwConsumer& operator=(const EtwConsumer&) = delete;
    EtwConsumer(EtwConsumer&&)                 = delete;
    EtwConsumer& operator=(EtwConsumer&&)      = delete;
    ~EtwConsumer()                             = default;
    std::mutex lock;
    std::vector<EtwRecord> records;
    size_t malformed = 0u;
};

// Reads little-endian fields in order from a TraceLogging payload.
class PayloadReader final
{
public:
    PayloadReader(const uint8_t* data, size_t size) noexcept : _data(data), _size(size)
    {
    }
    template <typename T> [[nodiscard]] T Read() noexcept
    {
        T value{};
        if (_offset + sizeof(T) > _size)
        {
            _failed = true;
            return value;
        }
        std::memcpy(&value, _data + _offset, sizeof(T));
        _offset += sizeof(T);
        return value;
    }
    [[nodiscard]] bool Complete() const noexcept
    {
        return ! _failed && _offset == _size;
    }

private:
    const uint8_t* _data;
    size_t _size;
    size_t _offset = 0u;
    bool _failed   = false;
};

// The event name from a TraceLogging event's self-describing metadata: a 16-bit size, tag bytes chained by their high bit,
// then the UTF-8 name.
[[nodiscard]] std::string TraceLoggingEventName(const EVENT_RECORD& event)
{
    for (USHORT index = 0u; index < event.ExtendedDataCount; ++index)
    {
        const EVENT_HEADER_EXTENDED_DATA_ITEM& item = event.ExtendedData[index];
        if (item.ExtType != EVENT_HEADER_EXT_TYPE_EVENT_SCHEMA_TL || item.DataSize < 3u)
            continue;
        const auto* bytes = reinterpret_cast<const uint8_t*>(item.DataPtr);
        size_t offset     = 2u;
        while (offset < item.DataSize && (bytes[offset] & 0x80u) != 0u)
            ++offset;
        ++offset;
        size_t end = offset;
        while (end < item.DataSize && bytes[end] != 0u)
            ++end;
        return std::string(reinterpret_cast<const char*>(bytes + offset), end - offset);
    }
    return {};
}

void WINAPI OnEtwEvent(PEVENT_RECORD event) noexcept
{
    auto& consumer = *static_cast<EtwConsumer*>(event->UserContext);
    if (IsEqualGUID(event->EventHeader.ProviderId, Trace::kProviderId) == FALSE || event->EventHeader.ProcessId != GetCurrentProcessId())
        return;
    EtwRecord record;
    try
    {
        record.name = TraceLoggingEventName(*event);
    }
    catch (...)
    {
        std::terminate();
    }
    record.threadId = event->EventHeader.ThreadId;
    PayloadReader reader(static_cast<const uint8_t*>(event->UserData), event->UserDataLength);
    record.hostId = reader.Read<uint32_t>();
    if (record.name == "InputReceived")
    {
        record.input  = reader.Read<uint64_t>();
        record.detail = reader.Read<uint8_t>();
    }
    else if (record.name == "FrameRequested")
    {
        record.frame  = reader.Read<uint64_t>();
        record.input  = reader.Read<uint64_t>();
        record.detail = reader.Read<uint8_t>();
    }
    else if (record.name == "PrepareBegin" || record.name == "Composite")
    {
        record.frame = reader.Read<uint64_t>();
        record.input = reader.Read<uint64_t>();
    }
    else if (record.name == "PrepareEnd")
    {
        record.frame = reader.Read<uint64_t>();
        record.input = reader.Read<uint64_t>();
        static_cast<void>(reader.Read<int32_t>());
        static_cast<void>(reader.Read<uint64_t>());
    }
    else if (record.name == "Present")
    {
        record.frame = reader.Read<uint64_t>();
        record.input = reader.Read<uint64_t>();
        static_cast<void>(reader.Read<uint8_t>());
        static_cast<void>(reader.Read<int32_t>());
    }
    else if (record.name == "Wake")
        record.detail = reader.Read<uint8_t>();
    else if (record.name == "FramePaced")
    {
        record.frame  = reader.Read<uint64_t>();
        record.detail = reader.Read<uint8_t>();
        static_cast<void>(reader.Read<uint64_t>()); // TargetQpc
        static_cast<void>(reader.Read<uint64_t>()); // SampleQpc
        static_cast<void>(reader.Read<uint64_t>()); // PeriodQpc
        static_cast<void>(reader.Read<uint8_t>());  // Clamped
    }
    record.qpc = reader.Read<uint64_t>();
    const std::lock_guard guard(consumer.lock);
    if (! reader.Complete())
    {
        ++consumer.malformed;
        return;
    }
    try
    {
        consumer.records.push_back(std::move(record));
    }
    catch (...)
    {
        std::terminate();
    }
}

[[nodiscard]] const char* EventName(Trace::Event event) noexcept
{
    switch (event)
    {
        case Trace::Event::InputReceived: return "InputReceived";
        case Trace::Event::FrameRequested: return "FrameRequested";
        case Trace::Event::PrepareBegin: return "PrepareBegin";
        case Trace::Event::PrepareEnd: return "PrepareEnd";
        case Trace::Event::Present: return "Present";
        case Trace::Event::Composite: return "Composite";
        case Trace::Event::Wake: return "Wake";
        case Trace::Event::HitReuseOff: return "HitReuseOff";
        case Trace::Event::FramePaced: return "FramePaced";
        default: return "";
    }
}

// A real-time session of this process's own, stopped however the test ends.
class RealTimeSession final
{
public:
    explicit RealTimeSession(std::wstring name) : _name(std::move(name)), _buffer(sizeof(EVENT_TRACE_PROPERTIES) + (_name.size() + 1u) * sizeof(wchar_t))
    {
        Stop(); // A session left behind by an earlier, killed run of this process id.
        auto& properties               = Properties();
        properties.Wnode.BufferSize    = static_cast<ULONG>(_buffer.size());
        properties.Wnode.Flags         = WNODE_FLAG_TRACED_GUID;
        properties.Wnode.ClientContext = 1u; // QueryPerformanceCounter
        properties.LogFileMode         = EVENT_TRACE_REAL_TIME_MODE;
        properties.LoggerNameOffset    = sizeof(EVENT_TRACE_PROPERTIES);
        _status                        = StartTraceW(&_handle, _name.c_str(), &properties);
    }
    ~RealTimeSession()
    {
        Stop();
    }
    RealTimeSession(const RealTimeSession&)            = delete;
    RealTimeSession& operator=(const RealTimeSession&) = delete;
    RealTimeSession(RealTimeSession&&)                 = delete;
    RealTimeSession& operator=(RealTimeSession&&)      = delete;

    [[nodiscard]] ULONG Status() const noexcept
    {
        return _status;
    }
    [[nodiscard]] TRACEHANDLE Handle() const noexcept
    {
        return _handle;
    }
    [[nodiscard]] const std::wstring& Name() const noexcept
    {
        return _name;
    }
    void Stop() noexcept
    {
        std::fill(_buffer.begin(), _buffer.end(), uint8_t{0u});
        auto& properties            = Properties();
        properties.Wnode.BufferSize = static_cast<ULONG>(_buffer.size());
        properties.LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        static_cast<void>(ControlTraceW(0u, _name.c_str(), &properties, EVENT_TRACE_CONTROL_STOP));
        _handle = 0u;
    }

private:
    EVENT_TRACE_PROPERTIES& Properties() noexcept
    {
        return *reinterpret_cast<EVENT_TRACE_PROPERTIES*>(_buffer.data());
    }
    std::wstring _name;
    std::vector<uint8_t> _buffer;
    TRACEHANDLE _handle = 0u;
    ULONG _status       = ERROR_SUCCESS;
};
} // namespace

void TestTraceProviderEventsReadBackThroughEtw()
{
    RealTimeSession session(L"DxUiTraceProviderTest-" + std::to_wstring(GetCurrentProcessId()));
    if (session.Status() == ERROR_ACCESS_DENIED || session.Status() == ERROR_NO_SYSTEM_RESOURCES)
    {
        const std::string reason = "ETW real-time session refused in this desktop session (StartTrace " + std::to_string(session.Status()) +
                                   "); the sink-based trace gate still applies";
        SkipDxUiTest(reason.c_str());
        return;
    }
    Require(session.Status() == ERROR_SUCCESS, "start a real-time ETW session");

    EtwConsumer consumer;
    EVENT_TRACE_LOGFILEW logFile{};
    logFile.LoggerName          = const_cast<wchar_t*>(session.Name().c_str());
    logFile.ProcessTraceMode    = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.EventRecordCallback = &OnEtwEvent;
    logFile.Context             = &consumer;
    const TRACEHANDLE trace     = OpenTraceW(&logFile);
    Require(trace != INVALID_PROCESSTRACE_HANDLE, "open the session for real-time consumption");
    std::thread reader([trace]() noexcept { static_cast<void>(ProcessTrace(const_cast<TRACEHANDLE*>(&trace), 1u, nullptr, nullptr)); });
    const auto finish = wil::scope_exit([&]() noexcept
    {
        session.Stop();
        if (reader.joinable())
            reader.join();
        static_cast<void>(CloseTrace(trace));
    });

    GridScrollScenario scenario;
    Require(EnableTraceEx2(session.Handle(), &Trace::kProviderId, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION, ~0ull, 0ull, 0u, nullptr) ==
                ERROR_SUCCESS,
            "enable the DxUi provider in the session");
    for (int attempt = 0; attempt < 500 && ! Trace::SessionListening(); ++attempt)
        Sleep(10u);
    Require(Trace::SessionListening(), "the provider's enable callback reports the listening session");

    RecordCapture capture(4096u);
    {
        const ScopedSink sink(capture);
        for (int notch = 0; notch < 10; ++notch)
        {
            scenario.Wheel();
            scenario.Paint();
        }
        scenario.Key(VK_DOWN);
        scenario.Paint();
    }
    Require(EnableTraceEx2(session.Handle(), &Trace::kProviderId, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0u, 0ull, 0ull, 0u, nullptr) == ERROR_SUCCESS,
            "disable the DxUi provider");
    for (int attempt = 0; attempt < 500 && Trace::SessionListening(); ++attempt)
        Sleep(10u);
    Require(! Trace::SessionListening(), "disabling the provider ends the listening state");
    session.Stop();
    reader.join();

    Require(capture.overflow == 0u && ! capture.records.empty(), "the sink captured the scenario");
    const uint64_t first = capture.records.front().qpcTimestamp;
    const uint64_t last  = capture.records.back().qpcTimestamp;
    std::vector<EtwRecord> etw;
    {
        const std::lock_guard guard(consumer.lock);
        Require(consumer.malformed == 0u, "every DxUi event read back has the documented payload");
        for (const EtwRecord& record : consumer.records)
            if (record.qpc >= first && record.qpc <= last)
                etw.push_back(record);
    }
    std::ranges::stable_sort(etw, {}, &EtwRecord::qpc);
    std::cerr << "    [TRACE] sink records=" << capture.records.size() << " ETW records in the same span=" << etw.size() << '\n';
    Require(etw.size() == capture.records.size(), "ETW delivers the same number of events as the sink received");
    for (size_t index = 0u; index < etw.size() && index < capture.records.size(); ++index)
    {
        const Trace::Record& expected = capture.records[index];
        const EtwRecord& actual       = etw[index];
        Require(actual.name == EventName(expected.event) && actual.hostId == expected.hostId && actual.frame == expected.frame &&
                    actual.input == expected.input && actual.detail == expected.detail && actual.qpc == expected.qpcTimestamp &&
                    actual.threadId == expected.threadId,
                "each ETW event equals the sink's record: name, host, frame, input, detail, timestamp and thread");
    }
}
