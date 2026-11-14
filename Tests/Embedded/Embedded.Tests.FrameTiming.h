#pragma once
// Fixture-agnostic frame timing method (Specs/Core/Core_PerformanceAndResources.md, "Frame timing method").
// Test fixtures only: the D3D11 timestamp queries, the staging pixel and the thread pinning below never enter the
// library's rendering paths. This header is not a paired-benchmark input: older baseline trees never compile it,
// so it may use the current library freely. Requires GraphicsFixture, Check and Hr from its includer.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
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

struct Pinning
{
    std::string requested;
    bool pinned            = false;
    DWORD processor        = 0u;
    DWORD_PTR previousMask = 0u;
    BYTE efficiencyClass   = 0u;
    BYTE maximumEfficiency = 0u;
};

// Choose a logical processor of the highest efficiency class in group 0, preferring the highest index (processor 0
// services the most interrupts). Only the measuring UI thread is pinned; driver threads stay unrestricted.
inline Pinning PinMeasuringThread(std::wstring_view request)
{
    Pinning result;
    result.requested = request == L"none" ? "none" : request == L"auto" ? "auto" : "explicit";
    if (request == L"none")
        return result;
    ULONG length = 0u;
    static_cast<void>(GetSystemCpuSetInformation(nullptr, 0u, &length, GetCurrentProcess(), 0u));
    Check(length > 0u, "frame timing CPU set inventory size");
    std::vector<std::byte> buffer(length);
    Check(GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data()), length, &length, GetCurrentProcess(), 0u) != FALSE,
          "frame timing CPU set inventory");
    bool found = false;
    for (ULONG offset = 0u; offset < length;)
    {
        const auto* entry = reinterpret_cast<const SYSTEM_CPU_SET_INFORMATION*>(buffer.data() + offset);
        Check(entry->Size > 0u, "frame timing CPU set entry size");
        if (entry->Type == CpuSetInformation && entry->CpuSet.Group == 0u && entry->CpuSet.LogicalProcessorIndex < 64u)
        {
            const BYTE index         = entry->CpuSet.LogicalProcessorIndex;
            const BYTE efficiency    = entry->CpuSet.EfficiencyClass;
            result.maximumEfficiency = (std::max)(result.maximumEfficiency, efficiency);
            const bool explicitMatch = result.requested == "explicit" && std::to_wstring(static_cast<unsigned>(index)) == request;
            const bool better        = ! found || efficiency > result.efficiencyClass || (efficiency == result.efficiencyClass && index > result.processor);
            if ((result.requested == "auto" && better) || explicitMatch)
            {
                found                  = true;
                result.processor       = index;
                result.efficiencyClass = efficiency;
            }
        }
        offset += entry->Size;
    }
    Check(found, "frame timing pinned processor exists in group 0");
    result.previousMask = SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << result.processor);
    Check(result.previousMask != 0u, "frame timing thread affinity");
    result.pinned = true;
    return result;
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
    size_t disjointFrames     = 0u;
    UINT64 timestampFrequency = 0u;
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
    Samples samples;
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

    const auto started = Clock::now();
    size_t frame       = 0u;
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
        auto& slot = slots[frame % kGpuSlots];
        Check(! slot.pending, "frame timing slot was read before reuse");
        const auto submitStart = Clock::now();
        fixture.Update(options.warmupFrames + frame, options.phase);
        gpu.context->Begin(slot.disjoint.get());
        gpu.context->End(slot.begin.get());
        const auto prepareStart = ThreadCycles();
        Hr(fixture.Prepare(), "frame timing preparation");
        samples.prepareCycles[frame] = ThreadCycles() - prepareStart;
        gpu.context->End(slot.prepared.get());
        const auto compositeStart = ThreadCycles();
        gpu.Bind();
        Hr(fixture.Composite(gpu.context.get(), viewport), "frame timing composition");
        samples.compositeCycles[frame] = ThreadCycles() - compositeStart;
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
    for (auto& slot : slots)
        if (slot.pending)
            read(slot, frame);
    return run;
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
           << "\",\"dedicatedVideoBytes\":" << run.adapter.videoBytes << "},\"pinning\":{\"requested\":" << JsonString(run.pinning.requested)
           << ",\"pinned\":" << (run.pinning.pinned ? "true" : "false") << ",\"processor\":" << run.pinning.processor
           << ",\"efficiencyClass\":" << +run.pinning.efficiencyClass << ",\"maximumEfficiencyClass\":" << +run.pinning.maximumEfficiency
           << ",\"scope\":\"measuring UI thread only\"}"
           << ",\"warmupFrames\":" << run.options.warmupFrames << ",\"minimumFrames\":" << run.options.minimumFrames
           << ",\"minimumSeconds\":" << run.options.minimumSeconds << ",\"frames\":" << run.frames << ",\"elapsedSeconds\":" << run.elapsedSeconds
           << ",\"gpuSlots\":" << kGpuSlots << ",\"gpuReadLatencyFrames\":1,\"timestampFrequency\":" << s.timestampFrequency
           << ",\"disjointFrames\":" << s.disjointFrames
           << ",\"cpuClock\":\"QueryThreadCycleTime (measuring UI thread)\",\"wallClock\":\"std::chrono::steady_clock\",\"samples\":{\"frames\":" << run.frames;
    series("prepareCycles", s.prepareCycles);
    series("compositeCycles", s.compositeCycles);
    series("submitUs", s.submitUs);
    series("gpuPrepareUs", s.gpuPrepareUs);
    series("gpuCompositeUs", s.gpuCompositeUs);
    series("gpuWaitUs", s.gpuWaitUs);
    series("gpuValid", s.gpuValid);
    series("gpuReadAfterSubmitted", s.gpuReadAfterSubmitted);
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
    }
}
} // namespace FrameTiming
#pragma warning(pop)
