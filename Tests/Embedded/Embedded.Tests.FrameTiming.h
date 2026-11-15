#pragma once
// Fixture-agnostic frame timing method (Specs/Core/Core_PerformanceAndResources.md, "Frame timing method").
// Test fixtures only: the D3D11 timestamp queries, the staging pixel and the thread pinning below never enter the
// library's rendering paths. This header is not a paired-benchmark input: older baseline trees never compile it,
// so it may use the current library freely. Requires GraphicsFixture, Check and Hr from its includer.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <intrin.h>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// Fixtures are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
namespace FrameTiming
{
inline constexpr std::string_view kMethod           = "dxui-frame-timing-v1";
inline constexpr size_t kJudgedMinimumFrames        = 1000u;
inline constexpr double kJudgedMinimumSeconds       = 2.0;
inline constexpr size_t kDefaultWarmupFrames        = 60u;
inline constexpr size_t kMaximumFrames              = 200000u;
inline constexpr size_t kGpuSlots                   = 2u;    // frame N is submitted before frame N-1 is read
inline constexpr unsigned kMaximumQueryDataAttempts = 4096u; // after the slot's staging map has completed

enum class Phase
{
    Clean,
    Dirty
};

// One measured workload: a performance fixture of the catalog (MakeFixture); the runner never changes.
class Fixture
{
public:
    Fixture()                                             = default;
    Fixture(const Fixture&)                               = delete;
    Fixture& operator=(const Fixture&)                    = delete;
    Fixture(Fixture&&)                                    = delete;
    Fixture& operator=(Fixture&&)                         = delete;
    virtual ~Fixture()                                    = default;
    virtual std::string_view Name() const noexcept        = 0;
    virtual std::string_view Description() const noexcept = 0;
    virtual UINT Width() const noexcept                   = 0;
    virtual UINT Height() const noexcept                  = 0;
    virtual float Dpi() const noexcept                    = 0;
    virtual HRESULT Initialize(ID3D11Device* device)      = 0;
    // Application work before preparation; a clean frame changes nothing.
    virtual void Update(size_t frame, Phase phase)                                          = 0;
    virtual HRESULT Prepare()                                                               = 0;
    virtual HRESULT Composite(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport) = 0;
    // Untimed work before frame `frame` (warm-up frames included): a fixture whose timed phase declares a finite number of
    // steps rebuilds its scene here when the next frame would start the phase again, so every timed frame is a declared step.
    virtual void BeforeFrame(size_t, Phase)
    {
    }
    // The declared steps of the timed dirty phase, played in a cycle; 0 when every frame index is a valid step.
    virtual size_t TimedSteps() const noexcept
    {
        return 0u;
    }
    // How many times BeforeFrame rebuilt the scene.
    virtual size_t Rebuilds() const noexcept
    {
        return 0u;
    }
};

// Every built catalog fixture on the embedded host, by its id ("FX5") or id and variant ("FX5@uhd-192dpi"); nullptr for any
// other name. Defined in Embedded.Tests.PerformanceFixtures.h, the fixture framework (plan W0.4).
inline std::unique_ptr<Fixture> MakeFixture(std::wstring_view name);
// True for a catalog fixture that only the interactive run executes (Definition::interactiveOnly), after printing
// "<id>: interactive, not run": the timing harness skips it and succeeds. Defined beside MakeFixture.
inline bool SkipsFixture(std::wstring_view name);

struct Options
{
    Phase phase           = Phase::Dirty;
    bool hardware         = true;
    std::wstring pinCore  = L"auto"; // auto, none or a logical processor index in group 0
    size_t warmupFrames   = kDefaultWarmupFrames;
    size_t minimumFrames  = kJudgedMinimumFrames;
    double minimumSeconds = kJudgedMinimumSeconds;
};

// A run ends once it holds the minimum frame count and the minimum duration, so a fast fixture still covers many
// scheduler quanta and a slow one still holds enough frames for a stable tail.
inline bool RunComplete(size_t frames, double elapsedSeconds, const Options& options) noexcept
{
    return frames >= kMaximumFrames || (frames >= options.minimumFrames && elapsedSeconds >= options.minimumSeconds);
}

// One logical processor of group 0, from the CPU set inventory: its physical core, efficiency class and last-level cache.
struct LogicalProcessor
{
    BYTE index           = 0u;
    BYTE core            = 0u;
    BYTE efficiencyClass = 0u;
    BYTE cache           = 0u;
};

inline std::vector<LogicalProcessor> ProcessorTopology()
{
    ULONG length = 0u;
    static_cast<void>(GetSystemCpuSetInformation(nullptr, 0u, &length, GetCurrentProcess(), 0u));
    Check(length > 0u, "frame timing CPU set inventory size");
    std::vector<std::byte> buffer(length);
    Check(GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data()), length, &length, GetCurrentProcess(), 0u) != FALSE,
          "frame timing CPU set inventory");
    std::vector<LogicalProcessor> processors;
    for (ULONG offset = 0u; offset < length;)
    {
        const auto* entry = reinterpret_cast<const SYSTEM_CPU_SET_INFORMATION*>(buffer.data() + offset);
        Check(entry->Size > 0u, "frame timing CPU set entry size");
        if (entry->Type == CpuSetInformation && entry->CpuSet.Group == 0u && entry->CpuSet.LogicalProcessorIndex < 64u)
            processors.push_back(
                {entry->CpuSet.LogicalProcessorIndex, entry->CpuSet.CoreIndex, entry->CpuSet.EfficiencyClass, entry->CpuSet.LastLevelCacheIndex});
        offset += entry->Size;
    }
    std::sort(processors.begin(), processors.end(), [](const LogicalProcessor& a, const LogicalProcessor& b) { return a.index < b.index; });
    Check(! processors.empty(), "frame timing finds logical processors in group 0");
    return processors;
}

// Cumulative idle and total (kernel, which includes idle, plus user) time of every logical processor of the calling
// thread's group, in 100 ns units, from NtQuerySystemInformation(SystemProcessorPerformanceInformation): no elevation.
struct ProcessorTimes
{
    std::vector<int64_t> idle, total;
};

inline ProcessorTimes SampleProcessorTimes()
{
    struct Entry
    {
        LARGE_INTEGER idle, kernel, user, reserved1[2];
        ULONG reserved2;
    };
    using Query        = LONG(WINAPI*)(ULONG, PVOID, ULONG, PULONG);
    static Query query = reinterpret_cast<Query>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation")));
    Check(query != nullptr, "frame timing processor time query");
    std::vector<Entry> entries(64u);
    ULONG returned = 0u;
    Check(query(8u, entries.data(), static_cast<ULONG>(entries.size() * sizeof(Entry)), &returned) >= 0, "frame timing processor times");
    ProcessorTimes times;
    for (size_t index = 0u; index < returned / sizeof(Entry); ++index)
    {
        times.idle.push_back(entries[index].idle.QuadPart);
        times.total.push_back(entries[index].kernel.QuadPart + entries[index].user.QuadPart);
    }
    return times;
}

// The busy share (0 to 1) of every logical processor between two samples.
inline std::vector<double> ProcessorBusy(const ProcessorTimes& before, const ProcessorTimes& after)
{
    std::vector<double> busy(after.total.size(), 0.0);
    for (size_t index = 0u; index < busy.size() && index < before.total.size(); ++index)
    {
        const double total = static_cast<double>(after.total[index] - before.total[index]);
        const double idle  = static_cast<double>(after.idle[index] - before.idle[index]);
        busy[index]        = total > 0.0 ? (std::clamp)(1.0 - idle / total, 0.0, 1.0) : 0.0;
    }
    return busy;
}

struct Pinning
{
    std::string requested;
    bool pinned            = false;
    DWORD processor        = 0u;
    DWORD_PTR previousMask = 0u;
    BYTE efficiencyClass   = 0u;
    BYTE maximumEfficiency = 0u;
    BYTE core              = 0u;
    BYTE cache             = 0u;
    std::vector<DWORD> siblings;     // the other logical processors of the same physical core
    std::vector<double> sampledBusy; // auto: every processor's busy share over the selection sample
};

inline constexpr DWORD kQuietCoreSampleMilliseconds = 500u;

// The quietest physical core: of the highest efficiency class, not the core of processor 0 (which services the most
// interrupts) unless it is the only one, with the lowest busy share summed over its logical processors during a
// 500 ms sample (ties: the higher core index); the measuring thread runs on its lowest-numbered logical processor and
// its siblings, which share the core's execution resources, are recorded.
inline LogicalProcessor QuietestProcessor(const std::vector<LogicalProcessor>& processors, const std::vector<double>& busy)
{
    BYTE maximum = 0u;
    for (const auto& processor : processors)
        maximum = (std::max)(maximum, processor.efficiencyClass);
    const BYTE interruptCore = processors.front().core;
    bool otherCores          = false;
    for (const auto& processor : processors)
        otherCores = otherCores || (processor.core != interruptCore && processor.efficiencyClass == maximum);
    double bestScore             = 0.0;
    const LogicalProcessor* best = nullptr;
    for (const auto& candidate : processors)
    {
        if (candidate.efficiencyClass != maximum || (otherCores && candidate.core == interruptCore))
            continue;
        double score                  = 0.0;
        const LogicalProcessor* first = nullptr;
        for (const auto& member : processors)
            if (member.core == candidate.core)
            {
                score += member.index < busy.size() ? busy[member.index] : 0.0;
                if (! first)
                    first = &member;
            }
        if (first != &candidate)
            continue; // score each core once, from its lowest-numbered logical processor
        if (! best || score < bestScore - 1e-9 || (score <= bestScore + 1e-9 && candidate.core > best->core))
        {
            best      = &candidate;
            bestScore = score;
        }
    }
    Check(best != nullptr, "frame timing finds a core to pin");
    return *best;
}

inline Pinning PinMeasuringThread(std::wstring_view request)
{
    Pinning result;
    result.requested = request == L"none" ? "none" : request == L"auto" ? "auto" : "explicit";
    if (request == L"none")
        return result;
    const auto processors = ProcessorTopology();
    for (const auto& processor : processors)
        result.maximumEfficiency = (std::max)(result.maximumEfficiency, processor.efficiencyClass);
    LogicalProcessor chosen{};
    bool found = false;
    if (result.requested == "auto")
    {
        const auto before = SampleProcessorTimes();
        Sleep(kQuietCoreSampleMilliseconds);
        result.sampledBusy = ProcessorBusy(before, SampleProcessorTimes());
        chosen             = QuietestProcessor(processors, result.sampledBusy);
        found              = true;
    }
    else
        for (const auto& processor : processors)
            if (std::to_wstring(static_cast<unsigned>(processor.index)) == request)
            {
                chosen = processor;
                found  = true;
            }
    Check(found, "frame timing pinned processor exists in group 0");
    result.processor       = chosen.index;
    result.efficiencyClass = chosen.efficiencyClass;
    result.core            = chosen.core;
    result.cache           = chosen.cache;
    for (const auto& processor : processors)
        if (processor.core == chosen.core && processor.index != chosen.index)
            result.siblings.push_back(processor.index);
    result.previousMask = SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << result.processor);
    Check(result.previousMask != 0u, "frame timing thread affinity");
    result.pinned = true;
    return result;
}

// What else ran while the run was measured: per-processor busy shares, this process's own processor use, and the
// measuring thread's share of its processor (its cycle time against the time-stamp counter).
struct Interference
{
    bool tscAvailable  = false;
    double wallSeconds = 0.0;
    std::vector<double> processorBusy;
    double ownProcessors    = 0.0; // this process's CPU time over the wall time, in logical processors
    double threadShare      = 0.0; // measuring thread's cycles over TSC ticks for the whole loop (0 without a TSC)
    double measuringBusy    = 0.0;
    double siblingBusy      = 0.0; // the busiest sibling of the measuring processor
    double otherOnMeasuring = 0.0; // the measuring processor's busy share not spent by the measuring thread
    double otherProcessors  = 0.0; // all processors' busy time not spent by this process, in logical processors
};

inline uint64_t ProcessCpuTime()
{
    FILETIME created{}, exited{}, kernel{}, user{};
    Check(GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) != FALSE, "frame timing process times");
    const auto value = [](const FILETIME& time) { return (static_cast<uint64_t>(time.dwHighDateTime) << 32u) | time.dwLowDateTime; };
    return value(kernel) + value(user);
}

// The time-stamp counter on x64, which QueryThreadCycleTime counts in; no equivalent is read on ARM64.
inline bool ReadTsc(uint64_t& value) noexcept
{
#if defined(_M_X64)
    value = __rdtsc();
    return true;
#else
    value = 0u;
    return false;
#endif
}

inline std::string Utf8(std::wstring_view text)
{
    if (text.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    Check(size > 0, "frame timing UTF-8 size");
    std::string result(static_cast<size_t>(size), '\0');
    Check(WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr) == size,
          "frame timing UTF-8 conversion");
    return result;
}

inline std::string JsonString(std::string_view text)
{
    std::string result = "\"";
    for (const char c : text)
    {
        if (c == '"' || c == '\\')
            result += '\\';
        if (static_cast<unsigned char>(c) < 0x20u)
            result += ' ';
        else
            result += c;
    }
    return result + '"';
}

struct AdapterIdentity
{
    std::string description;
    UINT vendorId     = 0u;
    UINT deviceId     = 0u;
    bool software     = false;
    uint64_t driver   = 0u;
    SIZE_T videoBytes = 0u;
    LUID luid{};
};

inline AdapterIdentity DescribeAdapter(ID3D11Device* device)
{
    AdapterIdentity identity;
    wil::com_ptr_nothrow<IDXGIDevice> dxgi;
    Hr(device->QueryInterface(IID_PPV_ARGS(dxgi.put())), "frame timing DXGI device");
    wil::com_ptr_nothrow<IDXGIAdapter> adapter;
    Hr(dxgi->GetAdapter(adapter.put()), "frame timing adapter");
    wil::com_ptr_nothrow<IDXGIAdapter1> adapter1;
    Hr(adapter->QueryInterface(IID_PPV_ARGS(adapter1.put())), "frame timing adapter identity");
    DXGI_ADAPTER_DESC1 desc{};
    Hr(adapter1->GetDesc1(&desc), "frame timing adapter description");
    identity.description = Utf8(desc.Description);
    identity.luid        = desc.AdapterLuid;
    identity.vendorId    = desc.VendorId;
    identity.deviceId    = desc.DeviceId;
    identity.software    = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u;
    identity.videoBytes  = desc.DedicatedVideoMemory;
    LARGE_INTEGER version{};
    if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version)))
        identity.driver = static_cast<uint64_t>(version.QuadPart);
    return identity;
}

// GPU time through a disjoint query and three timestamps (begin, after preparation, after composition). A slot is
// read only after the next frame has been submitted; the blocking wait is a map of the slot's own staging pixel,
// copied after that frame, so the CPU never waits for the frame it has just submitted.
struct GpuSlot
{
    wil::com_ptr_nothrow<ID3D11Query> disjoint, begin, prepared, composited;
    wil::com_ptr_nothrow<ID3D11Texture2D> completion;
    size_t frame = 0u;
    bool pending = false;
};

struct Samples
{
    std::vector<uint64_t> prepareCycles, compositeCycles;
    std::vector<double> submitUs, gpuPrepareUs, gpuCompositeUs, gpuWaitUs;
    std::vector<uint8_t> gpuValid;
    std::vector<size_t> gpuReadAfterSubmitted; // frames submitted when each frame's GPU result was read
    // The measuring thread's cycles over TSC ticks during preparation and composition, in 1/10,000: below 10,000, the
    // thread lost processor time inside the frame (preempted or waiting). Zero throughout without a TSC (ARM64).
    std::vector<uint16_t> runningPermyriad;
    size_t disjointFrames     = 0u;
    UINT64 timestampFrequency = 0u;
    size_t preparedFrames     = 0u; // timed frames whose Prepare did work (S_OK); S_FALSE means it found nothing to prepare
};

struct Run
{
    std::string fixture, fixtureDescription;
    UINT width = 0u, height = 0u;
    float dpi = 96.0f;
    Options options;
    Pinning pinning;
    AdapterIdentity adapter;
    size_t frames         = 0u;
    double elapsedSeconds = 0.0;
    size_t timedSteps     = 0u;
    size_t rebuilds       = 0u;
    Samples samples;
    Interference interference;
};

inline uint64_t ThreadCycles()
{
    ULONG64 cycles = 0u;
    Check(QueryThreadCycleTime(GetCurrentThread(), &cycles) != FALSE, "frame timing thread cycles");
    return cycles;
}

inline Run Measure(Fixture& fixture, const Options& options)
{
    Run run;
    run.fixture            = std::string(fixture.Name());
    run.fixtureDescription = std::string(fixture.Description());
    run.width              = fixture.Width();
    run.height             = fixture.Height();
    run.dpi                = fixture.Dpi();
    run.options            = options;
    run.pinning            = PinMeasuringThread(options.pinCore);
    const auto unpin       = wil::scope_exit([&]
    {
        if (run.pinning.pinned)
            static_cast<void>(SetThreadAffinityMask(GetCurrentThread(), run.pinning.previousMask));
    });
    GraphicsFixture gpu;
    gpu.width  = fixture.Width();
    gpu.height = fixture.Height();
    Hr(gpu.Create(! options.hardware), "frame timing device");
    run.adapter = DescribeAdapter(gpu.device.get());
    Check(! options.hardware || ! run.adapter.software, "frame timing hardware adapter is not a software rasterizer");
    Hr(fixture.Initialize(gpu.device.get()), "frame timing fixture");

    std::array<GpuSlot, kGpuSlots> slots;
    D3D11_TEXTURE2D_DESC pixel{};
    pixel.Width = pixel.Height = pixel.MipLevels = pixel.ArraySize = pixel.SampleDesc.Count = 1u;
    pixel.Format                                                                            = DXGI_FORMAT_B8G8R8A8_UNORM;
    pixel.Usage                                                                             = D3D11_USAGE_STAGING;
    pixel.CPUAccessFlags                                                                    = D3D11_CPU_ACCESS_READ;
    for (auto& slot : slots)
    {
        D3D11_QUERY_DESC query{D3D11_QUERY_TIMESTAMP_DISJOINT, 0u};
        Hr(gpu.device->CreateQuery(&query, slot.disjoint.put()), "frame timing disjoint query");
        query.Query = D3D11_QUERY_TIMESTAMP;
        Hr(gpu.device->CreateQuery(&query, slot.begin.put()), "frame timing begin timestamp");
        Hr(gpu.device->CreateQuery(&query, slot.prepared.put()), "frame timing preparation timestamp");
        Hr(gpu.device->CreateQuery(&query, slot.composited.put()), "frame timing composition timestamp");
        Hr(gpu.device->CreateTexture2D(&pixel, nullptr, slot.completion.put()), "frame timing completion pixel");
    }

    using Clock        = std::chrono::steady_clock;
    const auto elapsed = [](Clock::time_point start) { return std::chrono::duration<double, std::micro>(Clock::now() - start).count(); };
    auto& samples      = run.samples;
    samples.prepareCycles.reserve(options.minimumFrames);
    samples.compositeCycles.reserve(options.minimumFrames);
    samples.submitUs.reserve(options.minimumFrames);
    const D3D11_BOX corner{0u, 0u, 0u, 1u, 1u, 1u};
    const auto read = [&](GpuSlot& slot, size_t submitted)
    {
        const auto waitStart = Clock::now();
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Hr(gpu.context->Map(slot.completion.get(), 0u, D3D11_MAP_READ, 0u, &mapped), "frame timing late completion");
        gpu.context->Unmap(slot.completion.get(), 0u);
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
        HRESULT hr = S_FALSE;
        for (unsigned attempt = 0u; attempt < kMaximumQueryDataAttempts && hr == S_FALSE; ++attempt)
        {
            hr = gpu.context->GetData(slot.disjoint.get(), &disjoint, sizeof(disjoint), 0u);
            if (hr == S_FALSE)
                static_cast<void>(SwitchToThread());
        }
        Hr(hr, "frame timing disjoint data");
        Check(hr == S_OK, "frame timing disjoint data is available after completion");
        std::array<UINT64, 3> stamps{};
        const std::array<ID3D11Query*, 3> queries{slot.begin.get(), slot.prepared.get(), slot.composited.get()};
        for (size_t index = 0u; index < stamps.size(); ++index)
        {
            hr = S_FALSE;
            for (unsigned attempt = 0u; attempt < kMaximumQueryDataAttempts && hr == S_FALSE; ++attempt)
            {
                hr = gpu.context->GetData(queries[index], &stamps[index], sizeof(UINT64), 0u);
                if (hr == S_FALSE)
                    static_cast<void>(SwitchToThread());
            }
            Check(hr == S_OK, "frame timing timestamp data is available after completion");
        }
        samples.gpuWaitUs[slot.frame]             = elapsed(waitStart);
        samples.gpuReadAfterSubmitted[slot.frame] = submitted;
        const bool valid                          = ! disjoint.Disjoint && disjoint.Frequency > 0u && stamps[1] >= stamps[0] && stamps[2] >= stamps[1];
        if (! valid)
            ++samples.disjointFrames;
        else
        {
            if (samples.timestampFrequency == 0u)
                samples.timestampFrequency = disjoint.Frequency;
            const double toUs                  = 1.0e6 / static_cast<double>(disjoint.Frequency);
            samples.gpuPrepareUs[slot.frame]   = static_cast<double>(stamps[1] - stamps[0]) * toUs;
            samples.gpuCompositeUs[slot.frame] = static_cast<double>(stamps[2] - stamps[1]) * toUs;
            samples.gpuValid[slot.frame]       = 1u;
        }
        slot.pending = false;
    };

    const D3D11_VIEWPORT viewport = gpu.Viewport();
    for (size_t frame = 0u; frame < options.warmupFrames; ++frame)
    {
        fixture.BeforeFrame(frame, options.phase);
        fixture.Update(frame, options.phase);
        Hr(fixture.Prepare(), "frame timing warm preparation");
        gpu.Bind();
        Hr(fixture.Composite(gpu.context.get(), viewport), "frame timing warm composition");
    }
    {
        // Drain warm-up work so the first measured frame does not wait for it.
        auto& slot = slots[0];
        gpu.context->CopySubresourceRegion(slot.completion.get(), 0u, 0u, 0u, 0u, gpu.target.get(), 0u, &corner);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Hr(gpu.context->Map(slot.completion.get(), 0u, D3D11_MAP_READ, 0u, &mapped), "frame timing warm completion");
        gpu.context->Unmap(slot.completion.get(), 0u);
    }

    const auto started           = Clock::now();
    const auto processorsBefore  = SampleProcessorTimes();
    const uint64_t processBefore = ProcessCpuTime();
    ULONG64 processCyclesBefore  = 0u;
    Check(QueryProcessCycleTime(GetCurrentProcess(), &processCyclesBefore) != FALSE, "frame timing process cycles");
    const uint64_t threadBefore   = ThreadCycles();
    uint64_t tscBefore            = 0u;
    run.interference.tscAvailable = ReadTsc(tscBefore);
    size_t frame                  = 0u;
    while (! RunComplete(frame, elapsed(started) / 1.0e6, options))
    {
        samples.prepareCycles.push_back(0u);
        samples.compositeCycles.push_back(0u);
        samples.submitUs.push_back(0.0);
        samples.gpuPrepareUs.push_back(0.0);
        samples.gpuCompositeUs.push_back(0.0);
        samples.gpuWaitUs.push_back(0.0);
        samples.gpuValid.push_back(0u);
        samples.gpuReadAfterSubmitted.push_back(0u);
        samples.runningPermyriad.push_back(0u);
        auto& slot = slots[frame % kGpuSlots];
        Check(! slot.pending, "frame timing slot was read before reuse");
        fixture.BeforeFrame(options.warmupFrames + frame, options.phase);
        const auto submitStart = Clock::now();
        fixture.Update(options.warmupFrames + frame, options.phase);
        gpu.context->Begin(slot.disjoint.get());
        gpu.context->End(slot.begin.get());
        uint64_t prepareTsc = 0u, prepareTscEnd = 0u, compositeTsc = 0u, compositeTscEnd = 0u;
        static_cast<void>(ReadTsc(prepareTsc));
        const auto prepareStart      = ThreadCycles();
        const HRESULT prepared       = fixture.Prepare();
        samples.prepareCycles[frame] = ThreadCycles() - prepareStart;
        static_cast<void>(ReadTsc(prepareTscEnd));
        Hr(prepared, "frame timing preparation");
        samples.preparedFrames += prepared == S_OK ? 1u : 0u;
        gpu.context->End(slot.prepared.get());
        static_cast<void>(ReadTsc(compositeTsc));
        const auto compositeStart = ThreadCycles();
        gpu.Bind();
        Hr(fixture.Composite(gpu.context.get(), viewport), "frame timing composition");
        samples.compositeCycles[frame] = ThreadCycles() - compositeStart;
        static_cast<void>(ReadTsc(compositeTscEnd));
        if (run.interference.tscAvailable)
        {
            const double ticks              = static_cast<double>((prepareTscEnd - prepareTsc) + (compositeTscEnd - compositeTsc));
            const double cycles             = static_cast<double>(samples.prepareCycles[frame] + samples.compositeCycles[frame]);
            samples.runningPermyriad[frame] = static_cast<uint16_t>(ticks > 0.0 ? (std::min)(65535.0, std::round(10000.0 * cycles / ticks)) : 0.0);
        }
        gpu.context->End(slot.composited.get());
        gpu.context->End(slot.disjoint.get());
        gpu.context->CopySubresourceRegion(slot.completion.get(), 0u, 0u, 0u, 0u, gpu.target.get(), 0u, &corner);
        samples.submitUs[frame] = elapsed(submitStart);
        slot.frame              = frame;
        slot.pending            = true;
        ++frame;
        // One frame late: frame N-1 is read only after frame N has been submitted.
        auto& previous = slots[(frame + kGpuSlots - 2u) % kGpuSlots];
        if (frame >= 2u && previous.pending)
            read(previous, frame);
    }
    run.elapsedSeconds = elapsed(started) / 1.0e6;
    run.frames         = frame;
    {
        auto& interference         = run.interference;
        const uint64_t threadAfter = ThreadCycles();
        uint64_t tscAfter          = 0u;
        static_cast<void>(ReadTsc(tscAfter));
        interference.wallSeconds   = run.elapsedSeconds;
        interference.processorBusy = ProcessorBusy(processorsBefore, SampleProcessorTimes());
        // Process cycles over TSC ticks where the TSC is read (exact for short runs); otherwise process times, which
        // advance in scheduler ticks.
        ULONG64 processCyclesAfter = 0u;
        Check(QueryProcessCycleTime(GetCurrentProcess(), &processCyclesAfter) != FALSE, "frame timing process cycles");
        interference.ownProcessors = interference.tscAvailable && tscAfter > tscBefore
                                         ? static_cast<double>(processCyclesAfter - processCyclesBefore) / static_cast<double>(tscAfter - tscBefore)
                                         : static_cast<double>(ProcessCpuTime() - processBefore) / 1.0e7 / (std::max)(1e-9, interference.wallSeconds);
        interference.threadShare   = interference.tscAvailable && tscAfter > tscBefore
                                         ? (std::min)(1.0, static_cast<double>(threadAfter - threadBefore) / static_cast<double>(tscAfter - tscBefore))
                                         : 0.0;
        const auto busy            = [&](DWORD index) { return index < interference.processorBusy.size() ? interference.processorBusy[index] : 0.0; };
        if (run.pinning.pinned)
        {
            interference.measuringBusy    = busy(run.pinning.processor);
            interference.otherOnMeasuring = interference.tscAvailable ? (std::max)(0.0, interference.measuringBusy - interference.threadShare) : 0.0;
            for (const DWORD sibling : run.pinning.siblings)
                interference.siblingBusy = (std::max)(interference.siblingBusy, busy(sibling));
        }
        double total = 0.0;
        for (const double value : interference.processorBusy)
            total += value;
        interference.otherProcessors = (std::max)(0.0, total - interference.ownProcessors);
    }
    run.timedSteps = fixture.TimedSteps();
    run.rebuilds   = fixture.Rebuilds();
    for (auto& slot : slots)
        if (slot.pending)
            read(slot, frame);
    return run;
}

// The adapter LUID as the "GPU Engine" performance counters spell it in their instance names: 0xHIGH_0xLOW.
inline std::string LuidText(const LUID& luid)
{
    std::ostringstream text;
    text << "0x" << std::hex << std::setw(8) << std::setfill('0') << static_cast<unsigned long>(luid.HighPart) << "_0x" << std::setw(8) << luid.LowPart;
    return text.str();
}

inline void Write(const Run& run, std::ostream& output)
{
    const auto& s     = run.samples;
    const auto series = [&](const char* name, const auto& values)
    {
        output << ",\"" << name << "\":[";
        for (size_t index = 0u; index < values.size(); ++index)
            output << (index ? "," : "") << +values[index];
        output << ']';
    };
    output << std::setprecision(10) << "{\"schema\":1,\"method\":" << JsonString(kMethod) << ",\"compiler\":" << _MSC_FULL_VER
           << ",\"fixture\":" << JsonString(run.fixture) << ",\"fixtureDescription\":" << JsonString(run.fixtureDescription) << ",\"width\":" << run.width
           << ",\"height\":" << run.height << ",\"dpi\":" << run.dpi << ",\"phase\":\"" << (run.options.phase == Phase::Dirty ? "dirty" : "clean")
           << "\",\"renderer\":\"" << (run.options.hardware ? "Hardware" : "WARP") << "\",\"adapter\":{\"description\":" << JsonString(run.adapter.description)
           << ",\"vendorId\":" << run.adapter.vendorId << ",\"deviceId\":" << run.adapter.deviceId
           << ",\"software\":" << (run.adapter.software ? "true" : "false") << ",\"driverVersion\":\"" << (run.adapter.driver >> 48u) << '.'
           << ((run.adapter.driver >> 32u) & 0xFFFFu) << '.' << ((run.adapter.driver >> 16u) & 0xFFFFu) << '.' << (run.adapter.driver & 0xFFFFu)
           << "\",\"dedicatedVideoBytes\":" << run.adapter.videoBytes << ",\"luid\":\"" << LuidText(run.adapter.luid)
           << "\"},\"pinning\":{\"requested\":" << JsonString(run.pinning.requested) << ",\"pinned\":" << (run.pinning.pinned ? "true" : "false")
           << ",\"processor\":" << run.pinning.processor << ",\"efficiencyClass\":" << +run.pinning.efficiencyClass
           << ",\"maximumEfficiencyClass\":" << +run.pinning.maximumEfficiency << ",\"core\":" << +run.pinning.core
           << ",\"lastLevelCache\":" << +run.pinning.cache;
    series("siblings", run.pinning.siblings);
    series("sampledBusy", run.pinning.sampledBusy);
    const auto& interference = run.interference;
    output << ",\"selection\":"
           << JsonString(run.pinning.requested == "auto" ? "quietest physical core of the highest efficiency class over a 500 ms sample, not processor 0's "
                                                           "core; its lowest-numbered logical processor"
                                                         : run.pinning.requested)
           << ",\"scope\":\"measuring UI thread only\"},\"interference\":{\"tscAvailable\":" << (interference.tscAvailable ? "true" : "false")
           << ",\"wallSeconds\":" << interference.wallSeconds << ",\"logicalProcessors\":" << interference.processorBusy.size()
           << ",\"ownProcessors\":" << interference.ownProcessors << ",\"threadShare\":" << interference.threadShare
           << ",\"measuringBusy\":" << interference.measuringBusy << ",\"siblingBusy\":" << interference.siblingBusy
           << ",\"otherOnMeasuring\":" << interference.otherOnMeasuring << ",\"otherProcessors\":" << interference.otherProcessors;
    series("processorBusy", interference.processorBusy);
    output << '}' << ",\"warmupFrames\":" << run.options.warmupFrames << ",\"minimumFrames\":" << run.options.minimumFrames
           << ",\"minimumSeconds\":" << run.options.minimumSeconds << ",\"frames\":" << run.frames << ",\"elapsedSeconds\":" << run.elapsedSeconds
           << ",\"gpuSlots\":" << kGpuSlots << ",\"gpuReadLatencyFrames\":1,\"timestampFrequency\":" << s.timestampFrequency
           << ",\"disjointFrames\":" << s.disjointFrames << ",\"preparedFrames\":" << s.preparedFrames << ",\"timedSteps\":" << run.timedSteps
           << ",\"rebuilds\":" << run.rebuilds
           << ",\"cpuClock\":\"QueryThreadCycleTime (measuring UI thread)\",\"wallClock\":\"std::chrono::steady_clock\",\"samples\":{\"frames\":" << run.frames;
    series("prepareCycles", s.prepareCycles);
    series("compositeCycles", s.compositeCycles);
    series("submitUs", s.submitUs);
    series("gpuPrepareUs", s.gpuPrepareUs);
    series("gpuCompositeUs", s.gpuCompositeUs);
    series("gpuWaitUs", s.gpuWaitUs);
    series("gpuValid", s.gpuValid);
    series("gpuReadAfterSubmitted", s.gpuReadAfterSubmitted);
    series("runningPermyriad", s.runningPermyriad);
    output << "}}\n";
}

inline bool TryParseCount(std::wstring_view text, size_t& value)
{
    value = 0u;
    if (text.empty() || text.size() > 9u)
        return false;
    for (const wchar_t c : text)
    {
        if (c < L'0' || c > L'9')
            return false;
        value = value * 10u + static_cast<size_t>(c - L'0');
    }
    return true;
}

// DxUi.EmbeddedTests.exe --processor-topology <output>: group 0's logical processors (core, efficiency class, last-level
// cache, busy share over a 500 ms sample) and the quietest physical core, as -PinCore auto would choose it.
inline int TopologyCommand(int argc, wchar_t** argv)
{
    if (argc != 3)
    {
        std::fputs("Processor topology requires an output path.\n", stderr);
        return 2;
    }
    const auto processors = ProcessorTopology();
    const auto before     = SampleProcessorTimes();
    Sleep(kQuietCoreSampleMilliseconds);
    const auto busy   = ProcessorBusy(before, SampleProcessorTimes());
    const auto chosen = QuietestProcessor(processors, busy);
    std::ofstream output{std::filesystem::path(argv[2])};
    Check(bool(output), "processor topology output file");
    output << std::setprecision(6) << "{\"schema\":1,\"processors\":[";
    for (size_t index = 0u; index < processors.size(); ++index)
    {
        const auto& processor = processors[index];
        output << (index ? "," : "") << "{\"index\":" << +processor.index << ",\"core\":" << +processor.core
               << ",\"efficiencyClass\":" << +processor.efficiencyClass << ",\"lastLevelCache\":" << +processor.cache
               << ",\"busy\":" << (processor.index < busy.size() ? busy[processor.index] : 0.0) << '}';
    }
    output << "],\"quietest\":{\"processor\":" << +chosen.index << ",\"core\":" << +chosen.core << "}}\n";
    output.close();
    Check(bool(output), "processor topology written");
    return 0;
}

// DxUi.EmbeddedTests.exe --busy-load <i,j,...>: a synthetic background load for performance-timing.ps1 -Purpose
// LoadSensitivity. One thread per listed logical processor of group 0, pinned there, spins until the runner ends this
// process (its own child). Test tool only; never part of a measured run.
inline std::atomic<bool> busyLoadStop{false};
inline DWORD WINAPI BusyLoadThread(void*) noexcept
{
    volatile uint64_t counter = 0u;
    while (! busyLoadStop.load(std::memory_order_relaxed))
        counter = counter + 1u;
    return 0u;
}

inline int BusyLoadCommand(int argc, wchar_t** argv)
{
    std::vector<DWORD> processors;
    if (argc == 3)
    {
        std::wstring_view list(argv[2]);
        while (! list.empty())
        {
            const size_t comma = list.find(L',');
            size_t value       = 0u;
            if (! TryParseCount(list.substr(0u, comma), value) || value >= 64u)
            {
                processors.clear();
                break;
            }
            processors.push_back(static_cast<DWORD>(value));
            list = comma == std::wstring_view::npos ? std::wstring_view() : list.substr(comma + 1u);
        }
    }
    if (processors.empty())
    {
        std::fputs("Busy load requires a comma-separated list of logical processors of group 0.\n", stderr);
        return 2;
    }
    std::vector<wil::unique_handle> threads;
    for (const DWORD processor : processors)
    {
        wil::unique_handle thread(CreateThread(nullptr, 0u, BusyLoadThread, nullptr, CREATE_SUSPENDED, nullptr));
        Check(thread.is_valid(), "busy load thread");
        Check(SetThreadAffinityMask(thread.get(), DWORD_PTR{1} << processor) != 0u, "busy load thread affinity");
        Check(ResumeThread(thread.get()) != static_cast<DWORD>(-1), "busy load thread start");
        threads.push_back(std::move(thread));
    }
    std::cout << "busy load running on " << processors.size() << " logical processors\n" << std::flush;
    Sleep(INFINITE);
    return 0;
}

// DxUi.EmbeddedTests.exe --timing-method <output> <fixture> Clean|Dirty Hardware|Warp <auto|none|index> <minFrames> <minMilliseconds>
inline int RunCommand(int argc, wchar_t** argv)
{
    Options options;
    size_t milliseconds = 0u;
    if (argc != 9 || (std::wstring_view(argv[4]) != L"Clean" && std::wstring_view(argv[4]) != L"Dirty") ||
        (std::wstring_view(argv[5]) != L"Hardware" && std::wstring_view(argv[5]) != L"Warp") || ! TryParseCount(argv[7], options.minimumFrames) ||
        ! TryParseCount(argv[8], milliseconds) || options.minimumFrames == 0u)
    {
        std::fputs("Frame timing requires output, fixture, Clean|Dirty, Hardware|Warp, auto|none|processor, minimum frames and minimum milliseconds.\n",
                   stderr);
        return 2;
    }
    if (SkipsFixture(argv[3]))
        return 0;
    auto fixture = MakeFixture(argv[3]);
    if (! fixture)
    {
        std::fputs("Unknown frame timing fixture.\n", stderr);
        return 2;
    }
    options.phase          = std::wstring_view(argv[4]) == L"Dirty" ? Phase::Dirty : Phase::Clean;
    options.hardware       = std::wstring_view(argv[5]) == L"Hardware";
    options.pinCore        = argv[6];
    options.minimumSeconds = static_cast<double>(milliseconds) / 1000.0;
    const auto run         = Measure(*fixture, options);
    std::ofstream output{std::filesystem::path(argv[2])};
    Check(bool(output), "frame timing output file");
    Write(run, output);
    output.close();
    Check(bool(output), "frame timing report written");
    return 0;
}

// Deterministic WARP check of the method's mechanics, not of any timing value: separated cycle counters, GPU results
// read exactly one frame late, the frame/duration stopping rule and a well-formed report.
inline void SelfTest()
{
    Options rule;
    rule.minimumFrames  = 1000u;
    rule.minimumSeconds = 2.0;
    Check(! RunComplete(999u, 5.0, rule) && ! RunComplete(5000u, 1.9, rule) && RunComplete(1000u, 2.0, rule) && RunComplete(kMaximumFrames, 0.0, rule),
          "frame timing run needs both its minimum frames and its minimum duration");
    for (const auto phase : {Phase::Dirty, Phase::Clean})
    {
        const auto fixture = MakeFixture(L"FX5");
        Check(fixture != nullptr, "frame timing finds FX5 in the fixture catalog");
        Options options;
        options.phase          = phase;
        options.hardware       = false;
        options.pinCore        = L"none";
        options.warmupFrames   = 2u;
        options.minimumFrames  = 12u;
        options.minimumSeconds = 0.02;
        const auto run         = Measure(*fixture, options);
        const auto& s          = run.samples;
        Check(run.frames >= 12u && run.elapsedSeconds >= 0.02 && s.prepareCycles.size() == run.frames && s.gpuValid.size() == run.frames,
              "frame timing holds one sample per measured frame");
        Check(! run.adapter.software || run.adapter.vendorId == 0x1414u, "frame timing WARP identifies the Microsoft software adapter");
        for (size_t frame = 0u; frame < run.frames; ++frame)
        {
            Check(s.compositeCycles[frame] > 0u && s.submitUs[frame] > 0.0, "frame timing composition cycles and submission time are recorded");
            Check(phase == Phase::Clean || s.prepareCycles[frame] > 0u, "frame timing dirty preparation cycles are recorded");
            const size_t expected = frame + 1u < run.frames ? frame + 2u : run.frames;
            Check(s.gpuReadAfterSubmitted[frame] == expected, "frame timing reads each GPU result after the next frame is submitted");
        }
        Check(s.disjointFrames + static_cast<size_t>(std::count(s.gpuValid.begin(), s.gpuValid.end(), uint8_t{1})) == run.frames,
              "frame timing classifies every GPU result as valid or disjoint");
        std::ostringstream report;
        Write(run, report);
        const auto text = report.str();
        Check(text.find("\"method\":\"dxui-frame-timing-v1\"") != std::string::npos && text.find("\"gpuReadLatencyFrames\":1") != std::string::npos &&
                  text.back() == '\n',
              "frame timing report is complete");
        Check(run.timedSteps == 0u && run.rebuilds == 0u, "FX5's dirty phase repeats: its scene is never rebuilt");
        Check(phase == Phase::Clean || s.preparedFrames == run.frames, "every timed FX5 dirty frame prepares work");
        const auto& interference = run.interference;
        Check(! interference.processorBusy.empty() && interference.ownProcessors >= 0.0 && interference.wallSeconds > 0.0,
              "frame timing records every processor's busy share and its own processor use");
        Check(s.runningPermyriad.size() == run.frames, "frame timing records one running share per frame");
#if defined(_M_X64)
        Check(interference.tscAvailable && interference.threadShare > 0.0 && interference.threadShare <= 1.0 && interference.ownProcessors > 0.0,
              "frame timing measures the measuring thread's share of the time-stamp counter");
        for (const uint16_t share : s.runningPermyriad)
            Check(share > 0u, "every frame's running share is measured on x64");
#endif
    }
    // A finite timed phase (FX3: 100 wheel notches of a 2,048-row Grid, which reaches its end after about 675) is played as
    // declared: frames 100 and 200 rebuild the scene untimed, and every timed frame scrolls and prepares.
    // The quietest core is chosen per physical core, never processor 0's, and pinned on its lowest-numbered processor.
    const std::vector<LogicalProcessor> topology{
        {0u, 0u, 1u, 0u}, {1u, 0u, 1u, 0u}, {2u, 1u, 1u, 0u}, {3u, 1u, 1u, 0u}, {4u, 2u, 1u, 0u}, {5u, 2u, 1u, 0u}, {6u, 3u, 0u, 0u}, {7u, 3u, 0u, 0u}};
    const std::vector<double> sampled{0.0, 0.0, 0.5, 0.0, 0.05, 0.02, 0.0, 0.0};
    const auto quietest = QuietestProcessor(topology, sampled);
    Check(quietest.index == 4u && quietest.core == 2u, "frame timing pins the quietest core of the highest class, not processor 0's");
    const auto processors = ProcessorTopology();
    const auto pinned     = PinMeasuringThread(std::to_wstring(static_cast<unsigned>(processors.back().index)));
    Check(pinned.pinned && SetThreadAffinityMask(GetCurrentThread(), pinned.previousMask) != 0u, "frame timing pins and restores an explicit processor");
    for (const DWORD sibling : pinned.siblings)
        Check(std::any_of(processors.begin(), processors.end(), [&](const LogicalProcessor& p) { return p.index == sibling && p.core == pinned.core; }),
              "a recorded sibling shares the pinned processor's core");
    const auto grid = MakeFixture(L"FX3");
    Check(grid != nullptr, "frame timing finds FX3 in the fixture catalog");
    Options options;
    options.hardware       = false;
    options.pinCore        = L"none";
    options.warmupFrames   = 2u;
    options.minimumFrames  = 210u;
    options.minimumSeconds = 0.0;
    const auto run         = Measure(*grid, options);
    Check(run.frames == 210u && run.timedSteps == 100u && run.rebuilds == 2u, "FX3 replays its 100 declared wheel notches, rebuilding between passes");
    Check(run.samples.preparedFrames == run.frames, "every timed FX3 frame is a wheel notch that prepares work");
}
} // namespace FrameTiming
#pragma warning(pop)
