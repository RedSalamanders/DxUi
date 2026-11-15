#pragma once
// Hardware attribution method (plan W0.7; Specs/Core/Core_PerformanceAndResources.md, "Hardware attribution method"): where the
// UI thread's frame time goes, without kernel tracing. DxUi.EmbeddedTests.exe --frame-attribution runs one pass of one
// catalog fixture on the frame timing method's loop (FrameTiming::Measure), one fresh process per pass:
// - timing: the frame timing method unchanged (per-frame UI-thread cycles of preparation and composition, GPU timestamps
//   around them): the frame's cost, with no instrumentation inside it;
// - phases: the counting COM wrappers installed with a CallTicks record (Support.Tests.CallTicks.h): time inside BeginDraw,
//   EndDraw, Flush, every Direct2D draw call and DirectWrite layout and format creation, and the W0.1 counters, per frame;
//   the rest of preparation is DxUi's own code (with the C++ runtime and system calls it makes);
// - sampling: a sampler thread of this test process suspends the measuring thread about every millisecond while it is inside
//   preparation or composition, reads its instruction pointer and attributes the sample to the loaded module holding it
//   (DxUi, d2d1, dwrite, d3d11 with dxgi and the driver, C++ runtime, system, other). It is in-process user-mode sampling of
//   the leaf module: it needs no kernel-trace rights, sees no kernel stacks and attributes no callee time to a caller.
// Test fixtures only: the sampler thread, the wrappers and the timers never enter the library, whose hosts own no thread or
// timer. Requires FrameTiming, PerformanceFixtures (MakeAttributionFixture), CountingWrappers, Check and Hr from its includer.
#include "Support.Tests.CallTicks.h"
#include "Support.Tests.GpuEngineTime.h"

#include <psapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cwctype>
#include <optional>
#include <thread>

#pragma comment(lib, "psapi.lib")

// Fixtures, samplers and wrappers are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
namespace FrameAttribution
{
inline constexpr std::string_view kMethod          = "dxui-frame-attribution-v1";
inline constexpr DWORD kSampleIntervalMicroseconds = 1000u;

enum class Pass
{
    Timing,
    Phases,
    Sampling,
};

[[nodiscard]] inline const char* PassName(Pass pass) noexcept
{
    return pass == Pass::Timing ? "timing" : pass == Pass::Phases ? "phases" : "sampling";
}

// Where the measuring thread is, for the sampler: outside a frame's measured work, inside preparation or inside composition.
enum FramePhase : uint32_t
{
    kOutside   = 0u,
    kPrepare   = 1u,
    kComposite = 2u,
};

// The module classes of the attribution, in report order.
enum class ModuleClass : uint8_t
{
    DxUi,        // this executable: the statically linked library (and the fixture's own model code it calls back)
    D2d1,        // d2d1.dll
    DWrite,      // dwrite.dll and textshaping.dll
    D3d11Driver, // d3d11.dll, dxgi.dll, the WARP rasterizer and any module under the driver store (the user-mode driver)
    Crt,         // the C and C++ runtime DLLs
    System,      // ntdll, kernelbase, kernel32, win32u: system calls and waits land here
    Other,
    Count,
};

[[nodiscard]] inline const char* ModuleClassName(ModuleClass value) noexcept
{
    switch (value)
    {
        case ModuleClass::DxUi: return "dxui";
        case ModuleClass::D2d1: return "d2d1";
        case ModuleClass::DWrite: return "dwrite";
        case ModuleClass::D3d11Driver: return "d3d11AndDriver";
        case ModuleClass::Crt: return "crt";
        case ModuleClass::System: return "system";
        case ModuleClass::Other:
        case ModuleClass::Count:
        default: break;
    }
    return "other";
}

// Classifies a module by its lowercase file name and lowercase full path; `isExecutable` is this process's own image.
[[nodiscard]] inline ModuleClass ClassifyModule(std::wstring_view name, std::wstring_view path, bool isExecutable) noexcept
{
    if (isExecutable)
        return ModuleClass::DxUi;
    if (name == L"d2d1.dll")
        return ModuleClass::D2d1;
    if (name == L"dwrite.dll" || name == L"textshaping.dll")
        return ModuleClass::DWrite;
    if (name == L"d3d11.dll" || name == L"dxgi.dll" || name == L"d3d10warp.dll" || name == L"dxcore.dll" ||
        path.find(L"\\driverstore\\") != std::wstring_view::npos)
        return ModuleClass::D3d11Driver;
    if (name == L"ucrtbase.dll" || name.starts_with(L"vcruntime140") || name.starts_with(L"msvcp140") || name.starts_with(L"ucrtbased") ||
        name.starts_with(L"clang_rt.asan"))
        return ModuleClass::Crt;
    if (name == L"ntdll.dll" || name == L"kernelbase.dll" || name == L"kernel32.dll" || name == L"win32u.dll")
        return ModuleClass::System;
    return ModuleClass::Other;
}

inline std::wstring Lowercase(std::wstring_view text)
{
    std::wstring result(text);
    for (wchar_t& c : result)
        c = static_cast<wchar_t>(std::towlower(c));
    return result;
}

// The loaded modules of this process, sorted by base address, for instruction pointer lookups.
class ModuleMap final
{
public:
    struct Module
    {
        uintptr_t begin = 0u;
        uintptr_t end   = 0u;
        std::string name;
        ModuleClass moduleClass = ModuleClass::Other;
    };

    void Refresh()
    {
        modules_.clear();
        std::vector<HMODULE> handles(1024u);
        DWORD needed = 0u;
        Check(EnumProcessModules(GetCurrentProcess(), handles.data(), static_cast<DWORD>(handles.size() * sizeof(HMODULE)), &needed) != FALSE,
              "frame attribution module list");
        handles.resize((std::min)(handles.size(), static_cast<size_t>(needed) / sizeof(HMODULE)));
        const HMODULE executable = GetModuleHandleW(nullptr);
        for (const HMODULE handle : handles)
        {
            MODULEINFO info{};
            std::array<wchar_t, MAX_PATH> path{};
            if (! GetModuleInformation(GetCurrentProcess(), handle, &info, sizeof(info)) ||
                GetModuleFileNameW(handle, path.data(), static_cast<DWORD>(path.size())) == 0u)
                continue;
            const std::wstring fullPath = Lowercase(path.data());
            const size_t slash          = fullPath.find_last_of(L'\\');
            const std::wstring name     = slash == std::wstring::npos ? fullPath : fullPath.substr(slash + 1u);
            Module module;
            module.begin       = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
            module.end         = module.begin + info.SizeOfImage;
            module.name        = FrameTiming::Utf8(name);
            module.moduleClass = ClassifyModule(name, fullPath, handle == executable);
            modules_.push_back(std::move(module));
        }
        std::sort(modules_.begin(), modules_.end(), [](const Module& left, const Module& right) { return left.begin < right.begin; });
    }
    // The index of the module holding `address`, or modules().size() when none does.
    [[nodiscard]] size_t Find(uintptr_t address) const noexcept
    {
        const auto after =
            std::upper_bound(modules_.begin(), modules_.end(), address, [](uintptr_t value, const Module& module) { return value < module.begin; });
        if (after == modules_.begin())
            return modules_.size();
        const auto candidate = std::prev(after);
        return address < candidate->end ? static_cast<size_t>(candidate - modules_.begin()) : modules_.size();
    }
    [[nodiscard]] const std::vector<Module>& Modules() const noexcept
    {
        return modules_;
    }

private:
    std::vector<Module> modules_;
};

// Samples the measuring thread's instruction pointer about every kSampleIntervalMicroseconds while it is inside preparation or
// composition. Each sample suspends the thread, reads its context, its cycle count and the frame phase, and resumes it; nothing is
// allocated or locked while it is suspended. A sample counts as running when the thread consumed cycles since the previous
// sample, as waiting otherwise (a blocked thread's instruction pointer is at its system call).
class Sampler final
{
public:
    struct Tally
    {
        std::array<uint64_t, 3> running{}; // by FramePhase
        std::array<uint64_t, 3> waiting{};
    };

    explicit Sampler(std::atomic<uint32_t>& phase) : phase_(phase)
    {
    }
    ~Sampler()
    {
        Stop();
    }

    void Start()
    {
        map_.Refresh();
        tallies_.assign(map_.Modules().size() + 1u, Tally{}); // the last entry: no module (unknown)
        HANDLE duplicate = nullptr;
        Check(DuplicateHandle(GetCurrentProcess(),
                              GetCurrentThread(),
                              GetCurrentProcess(),
                              &duplicate,
                              THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                              FALSE,
                              0u) != FALSE,
              "frame attribution measuring thread handle");
        target_.reset(duplicate);
        stop_.create(wil::EventOptions::ManualReset);
        timer_.reset(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS));
        Check(timer_.is_valid(), "frame attribution high-resolution sampling timer");
        thread_ = std::thread([this] { Run(); });
    }
    void Stop()
    {
        if (! thread_.joinable())
            return;
        stop_.SetEvent();
        thread_.join();
    }

    [[nodiscard]] const ModuleMap& Map() const noexcept
    {
        return map_;
    }
    [[nodiscard]] const std::vector<Tally>& Tallies() const noexcept
    {
        return tallies_;
    }
    uint64_t outsideTicks   = 0u; // timer wakes while the thread was outside a frame's measured work
    uint64_t failedSamples  = 0u; // suspend or context read refused
    uint64_t unknownSamples = 0u; // instruction pointer in no loaded module (generated code)
    uint64_t wakes          = 0u;
    // The pinned measuring thread's processor, published by InstrumentedFixture; -1 until then.
    const std::atomic<int32_t>* measuringProcessor = nullptr;

private:
    // The sampler never runs on the measuring thread's physical core: once the pinned measuring thread has published its
    // processor (measuringProcessor), this thread's affinity excludes that core's logical processors, so the sampler's own
    // wakes add nothing to the measuring processor or its sibling.
    void KeepOffMeasuringCore()
    {
        const int32_t processor = measuringProcessor ? measuringProcessor->load(std::memory_order_acquire) : -1;
        if (processor < 0 || processor == movedOff_)
            return;
        movedOff_             = processor;
        DWORD_PTR processMask = 0u, systemMask = 0u;
        if (! GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask))
            return;
        const DWORD_PTR core = CoreMask(static_cast<DWORD>(processor));
        if ((processMask & ~core) != 0u)
            static_cast<void>(SetThreadAffinityMask(GetCurrentThread(), processMask & ~core));
    }
    // The logical processors of group 0 that share a physical core with processor (itself included).
    static DWORD_PTR CoreMask(DWORD processor)
    {
        DWORD length = 0u;
        static_cast<void>(GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length));
        std::vector<std::byte> buffer(length);
        if (length == 0u ||
            ! GetLogicalProcessorInformationEx(RelationProcessorCore, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &length))
            return DWORD_PTR{1} << processor;
        for (DWORD offset = 0u; offset < length;)
        {
            const auto* entry           = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset);
            const GROUP_AFFINITY& group = entry->Processor.GroupMask[0];
            if (group.Group == 0u && (group.Mask & (KAFFINITY{1} << processor)) != 0u)
                return static_cast<DWORD_PTR>(group.Mask);
            offset += entry->Size;
        }
        return DWORD_PTR{1} << processor;
    }
    void Run()
    {
        ULONG64 lastCycles = 0u;
        static_cast<void>(QueryThreadCycleTime(target_.get(), &lastCycles));
        const std::array<HANDLE, 2> handles{stop_.get(), timer_.get()};
        for (;;)
        {
            LARGE_INTEGER due{};
            due.QuadPart = -static_cast<LONGLONG>(kSampleIntervalMicroseconds) * 10;
            Check(SetWaitableTimer(timer_.get(), &due, 0, nullptr, nullptr, FALSE) != FALSE, "frame attribution sampling timer armed");
            if (WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, INFINITE) != WAIT_OBJECT_0 + 1u)
                return;
            ++wakes;
            KeepOffMeasuringCore();
            if (phase_.load(std::memory_order_acquire) == kOutside)
            {
                ++outsideTicks;
                static_cast<void>(QueryThreadCycleTime(target_.get(), &lastCycles));
                continue;
            }
            if (SuspendThread(target_.get()) == static_cast<DWORD>(-1))
            {
                ++failedSamples;
                continue;
            }
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;
            const BOOL read      = GetThreadContext(target_.get(), &context);
            ULONG64 cycles       = lastCycles;
            static_cast<void>(QueryThreadCycleTime(target_.get(), &cycles));
            const uint32_t phase = phase_.load(std::memory_order_acquire); // exact: the thread cannot change it while suspended
            static_cast<void>(ResumeThread(target_.get()));
            const bool running = cycles > lastCycles;
            lastCycles         = cycles;
            if (! read)
            {
                ++failedSamples;
                continue;
            }
            if (phase == kOutside)
            {
                ++outsideTicks;
                continue;
            }
#if defined(_M_X64)
            const auto address = static_cast<uintptr_t>(context.Rip);
#else
            const auto address = static_cast<uintptr_t>(context.Pc);
#endif
            size_t module = map_.Find(address);
            if (module == map_.Modules().size())
            {
                // A module loaded since the last refresh: refresh (the thread runs again, so allocation is safe), re-key the
                // tallies by module name (indices shift) and look again.
                const ModuleMap previous = map_;
                map_.Refresh();
                std::vector<Tally> rekeyed(map_.Modules().size() + 1u);
                for (size_t index = 0u; index < map_.Modules().size(); ++index)
                    for (size_t old = 0u; old < previous.Modules().size(); ++old)
                        if (previous.Modules()[old].begin == map_.Modules()[index].begin && previous.Modules()[old].name == map_.Modules()[index].name)
                            rekeyed[index] = tallies_[old];
                rekeyed.back() = tallies_.back();
                tallies_       = std::move(rekeyed);
                module         = map_.Find(address);
                if (module == map_.Modules().size())
                    ++unknownSamples;
            }
            auto& tally = tallies_[module];
            (running ? tally.running : tally.waiting)[phase] += 1u;
        }
    }

    std::atomic<uint32_t>& phase_;
    int32_t movedOff_ = -1;
    ModuleMap map_;
    std::vector<Tally> tallies_;
    wil::unique_handle target_;
    wil::unique_event stop_;
    wil::unique_handle timer_;
    std::thread thread_;
};

// GPU engine running time of this process (Support.Tests.GpuEngineTime.h, shared with the frame timing method).
using GpuRunningTime = NativeWorkTests::GpuEngineTime;

// What one measured frame did, beyond the frame timing method's own samples: ticks around preparation and composition (the
// measuring thread's CallTicks clock) and, in the phases pass, time inside the wrapped calls and the W0.1 counters.
struct FrameRecord
{
    uint64_t prepareTicks = 0u, compositeTicks = 0u;
    NativeWorkTests::CallTicks prepareCalls, compositeCalls;
    uint64_t drawCalls = 0u, paintExecutions = 0u, pixelsPainted = 0u, preparations = 0u, textLayoutsCreated = 0u, textFormatsCreated = 0u,
             implicitTextLayouts = 0u, createdResources = 0u;
    size_t framesSinceRebuild = 0u; // 0: the first frame after the fixture rebuilt its scene
};

[[nodiscard]] inline NativeWorkTests::CallTicks Subtract(const NativeWorkTests::CallTicks& later, const NativeWorkTests::CallTicks& earlier) noexcept
{
    return NativeWorkTests::CallTicks{later.beginDraw - earlier.beginDraw,
                                      later.endDraw - earlier.endDraw,
                                      later.flush - earlier.flush,
                                      later.drawCalls - earlier.drawCalls,
                                      later.textLayouts - earlier.textLayouts,
                                      later.textFormats - earlier.textFormats};
}

// A catalog fixture with the measuring thread's frame phase published for the sampler and each measured frame recorded.
class InstrumentedFixture final : public FrameTiming::Fixture
{
public:
    InstrumentedFixture(std::unique_ptr<FrameTiming::Fixture> inner, Pass pass, size_t warmupFrames, std::atomic<uint32_t>& phase)
        : inner_(std::move(inner)),
          pass_(pass),
          warmupFrames_(warmupFrames),
          phase_(phase)
    {
    }
    std::string_view Name() const noexcept override
    {
        return inner_->Name();
    }
    std::string_view Description() const noexcept override
    {
        return inner_->Description();
    }
    UINT Width() const noexcept override
    {
        return inner_->Width();
    }
    UINT Height() const noexcept override
    {
        return inner_->Height();
    }
    float Dpi() const noexcept override
    {
        return inner_->Dpi();
    }
    HRESULT Initialize(ID3D11Device* device) override
    {
        // The measured adapter, as the GPU Engine counters name it, for other processes' GPU busy time.
        if (device)
        {
            const std::string text = FrameTiming::LuidText(FrameTiming::DescribeAdapter(device).luid); // ASCII hex digits
            luid_.assign(text.begin(), text.end());
        }
        return inner_->Initialize(device);
    }
    // Untimed: a fixture whose timed phase is finite rebuilds its scene before the phase starts again. Each measured frame
    // records how many frames it follows the last rebuild, so the first frames after one can be told apart.
    void BeforeFrame(size_t frame, FrameTiming::Phase phase) override
    {
        const size_t before = inner_->Rebuilds();
        inner_->BeforeFrame(frame, phase);
        if (inner_->Rebuilds() != before)
            framesSinceRebuild_ = 0u;
    }
    size_t TimedSteps() const noexcept override
    {
        return inner_->TimedSteps();
    }
    size_t Rebuilds() const noexcept override
    {
        return inner_->Rebuilds();
    }
    void Update(size_t frame, FrameTiming::Phase phase) override
    {
        // The first measured frame: the device exists and the warm-up's GPU work has drained (FrameTiming::Measure). Outside
        // the frame's cycle brackets.
        if (frame == warmupFrames_ && gpuOpened_ == false)
        {
            gpuOpened_ = true;
            // FrameTiming::Measure has pinned this thread: the sampler keeps off its core from now on.
            measuringProcessor_.store(static_cast<int32_t>(GetCurrentProcessorNumber()), std::memory_order_release);
            if (gpu_.Open())
                gpuStart_ = gpu_.Read();
            if (allGpu_.Open(true))
            {
                allGpuStart_ = allGpu_.Read();
                static_cast<void>(QueryPerformanceCounter(&allGpuStartQpc_));
            }
        }
        inner_->Update(frame, phase);
    }
    // Other processes' share of the measured adapter's busiest engine over the measured frames (0..1), or a negative value
    // when the counters cannot say: the admission rule's otherGpuBusy. Instances present in one snapshot only are left out.
    [[nodiscard]] double MeasuredOtherGpuBusy()
    {
        const GpuRunningTime::Reading end = allGpu_.Read();
        LARGE_INTEGER now{}, frequency{};
        static_cast<void>(QueryPerformanceCounter(&now));
        static_cast<void>(QueryPerformanceFrequency(&frequency));
        const double seconds = static_cast<double>(now.QuadPart - allGpuStartQpc_.QuadPart) / static_cast<double>(frequency.QuadPart);
        if (! allGpuStart_.valid || ! end.valid || luid_.empty() || seconds <= 0.0)
            return -1.0;
        double busiest = 0.0;
        for (const auto& engine : GpuRunningTime::DeltaByEngine(allGpuStart_, end, luid_, GetCurrentProcessId(), {}))
            busiest = (std::max)(busiest, static_cast<double>(engine.others100ns) / 1.0e7 / seconds);
        return busiest;
    }
    // The GPU running time of the measured frames: read after FrameTiming::Measure has read every frame's GPU result.
    [[nodiscard]] GpuRunningTime::Reading MeasuredGpuRunningTime()
    {
        // This process's own engines only: an instance that appeared after the first reading started inside the window.
        return GpuRunningTime::Delta(gpuStart_, gpu_.Read(), GpuRunningTime::NewInstances::CountWhole);
    }
    HRESULT Prepare() override
    {
        const bool measured = calls_ >= warmupFrames_;
        if (measured)
            records_.emplace_back(); // reserved for kMaximumFrames: never reallocates inside a measured frame
        if (pass_ == Pass::Phases)
            workBefore_ = DxUi::Detail::Work();
        const NativeWorkTests::CallTicks calls = ticks_;
        // Whether the frame did work: one thread-local read before and after, in every pass.
        const uint64_t preparedBefore = DxUi::Detail::Work().frame.preparations;
        phase_.store(kPrepare, std::memory_order_release);
        const uint64_t start = NativeWorkTests::ReadCallTicks();
        const HRESULT result = inner_->Prepare();
        const uint64_t end   = NativeWorkTests::ReadCallTicks();
        phase_.store(kOutside, std::memory_order_release);
        if (measured)
        {
            FrameRecord& record       = records_.back();
            record.prepareTicks       = end - start;
            record.preparations       = DxUi::Detail::Work().frame.preparations - preparedBefore;
            record.framesSinceRebuild = framesSinceRebuild_;
            if (pass_ == Pass::Phases)
            {
                record.prepareCalls                = Subtract(ticks_, calls);
                const DxUi::Detail::WorkCounters d = DxUi::Detail::Subtract(DxUi::Detail::Work(), workBefore_);
                record.drawCalls += d.frame.drawCalls;
                record.paintExecutions += d.frame.paintExecutions;
                record.pixelsPainted += d.frame.pixelsPainted;
                record.textLayoutsCreated += d.resources.textLayout.created;
                record.textFormatsCreated += d.resources.textFormat.created;
                record.implicitTextLayouts += d.resources.implicitTextLayout.created;
                record.createdResources += DxUi::Detail::TotalCreated(d);
            }
        }
        return result;
    }
    HRESULT Composite(ID3D11DeviceContext* context, const D3D11_VIEWPORT& viewport) override
    {
        const bool measured                    = calls_++ >= warmupFrames_;
        const NativeWorkTests::CallTicks calls = ticks_;
        phase_.store(kComposite, std::memory_order_release);
        const uint64_t start = NativeWorkTests::ReadCallTicks();
        const HRESULT result = inner_->Composite(context, viewport);
        const uint64_t end   = NativeWorkTests::ReadCallTicks();
        phase_.store(kOutside, std::memory_order_release);
        if (measured)
        {
            records_.back().compositeTicks = end - start;
            if (pass_ == Pass::Phases)
                records_.back().compositeCalls = Subtract(ticks_, calls);
        }
        ++framesSinceRebuild_;
        return result;
    }

    // The measuring thread's processor once it is pinned (the sampler's Sampler::measuringProcessor), -1 before.
    std::atomic<int32_t> measuringProcessor_{-1};
    // The CallTicks record the phases pass installs on the measuring thread.
    NativeWorkTests::CallTicks ticks_;
    std::vector<FrameRecord> records_;

private:
    std::unique_ptr<FrameTiming::Fixture> inner_;
    Pass pass_;
    DxUi::Detail::WorkCounters workBefore_{};
    GpuRunningTime gpu_;
    GpuRunningTime::Reading gpuStart_;
    GpuRunningTime allGpu_;
    GpuRunningTime::Reading allGpuStart_;
    LARGE_INTEGER allGpuStartQpc_{};
    std::wstring luid_;
    bool gpuOpened_      = false;
    size_t warmupFrames_ = 0u;
    // Frames since the inner fixture last rebuilt its scene; a large value until the first rebuild.
    size_t framesSinceRebuild_ = static_cast<size_t>(-1) / 2u;
    size_t calls_              = 0u;
    std::atomic<uint32_t>& phase_;
};

struct Clock
{
    uint64_t ticks = 0u;
    LARGE_INTEGER qpc{};
};
inline Clock ReadClock() noexcept
{
    Clock clock;
    static_cast<void>(QueryPerformanceCounter(&clock.qpc));
    clock.ticks = NativeWorkTests::ReadCallTicks();
    return clock;
}

// One pass's report: the frame timing method's report of the same frames ("timing"), the per-frame records, the clock that
// converts ticks and thread cycles to time, and the pass's own results.
inline void WriteReport(std::ostream& output,
                        Pass pass,
                        std::wstring_view frame,
                        const FrameTiming::Run& run,
                        const InstrumentedFixture& fixture,
                        const Clock& begin,
                        const Clock& end,
                        const GpuRunningTime::Reading& gpu,
                        double otherGpuBusy,
                        const NativeWorkTests::CountingWrappers* wrappers,
                        const Sampler* sampler)
{
    LARGE_INTEGER frequency{};
    static_cast<void>(QueryPerformanceFrequency(&frequency));
    const double seconds        = static_cast<double>(end.qpc.QuadPart - begin.qpc.QuadPart) / static_cast<double>(frequency.QuadPart);
    const double ticksPerSecond = seconds > 0.0 ? static_cast<double>(end.ticks - begin.ticks) / seconds : 0.0;
    output << std::setprecision(10) << "{\"schema\":1,\"method\":" << FrameTiming::JsonString(kMethod) << ",\"pass\":\"" << PassName(pass)
           << "\",\"frame\":" << FrameTiming::JsonString(FrameTiming::Utf8(frame)) << ",\"configuration\":"
#if defined(_DEBUG)
           << "\"Debug\""
#else
           << "\"Release\""
#endif
           << ",\"tickSource\":\""
#if defined(_M_X64)
           << "rdtsc"
#else
           << "QueryPerformanceCounter"
#endif
           << "\",\"ticksPerSecond\":" << ticksPerSecond << ",\"clockSeconds\":" << seconds;
    const auto series = [&](const char* name, auto&& value)
    {
        output << ",\"" << name << "\":[";
        for (size_t index = 0u; index < fixture.records_.size(); ++index)
            output << (index ? "," : "") << value(fixture.records_[index]);
        output << ']';
    };
    output << ",\"frames\":{\"count\":" << fixture.records_.size();
    series("prepareTicks", [](const FrameRecord& r) { return r.prepareTicks; });
    series("compositeTicks", [](const FrameRecord& r) { return r.compositeTicks; });
    series("preparations", [](const FrameRecord& r) { return r.preparations; });
    series("framesSinceRebuild", [](const FrameRecord& r) { return (std::min)(r.framesSinceRebuild, size_t{1000000u}); });
    if (pass == Pass::Phases)
    {
        series("prepareBeginDrawTicks", [](const FrameRecord& r) { return r.prepareCalls.beginDraw; });
        series("prepareEndDrawTicks", [](const FrameRecord& r) { return r.prepareCalls.endDraw; });
        series("prepareFlushTicks", [](const FrameRecord& r) { return r.prepareCalls.flush; });
        series("prepareDrawCallTicks", [](const FrameRecord& r) { return r.prepareCalls.drawCalls; });
        series("prepareTextLayoutTicks", [](const FrameRecord& r) { return r.prepareCalls.textLayouts; });
        series("prepareTextFormatTicks", [](const FrameRecord& r) { return r.prepareCalls.textFormats; });
        series("compositeWrappedTicks",
               [](const FrameRecord& r)
        {
            const auto& c = r.compositeCalls;
            return c.beginDraw + c.endDraw + c.flush + c.drawCalls + c.textLayouts + c.textFormats;
        });
        series("drawCalls", [](const FrameRecord& r) { return r.drawCalls; });
        series("paintExecutions", [](const FrameRecord& r) { return r.paintExecutions; });
        series("pixelsPainted", [](const FrameRecord& r) { return r.pixelsPainted; });
        series("textLayoutsCreated", [](const FrameRecord& r) { return r.textLayoutsCreated; });
        series("textFormatsCreated", [](const FrameRecord& r) { return r.textFormatsCreated; });
        series("implicitTextLayouts", [](const FrameRecord& r) { return r.implicitTextLayouts; });
        series("createdResources", [](const FrameRecord& r) { return r.createdResources; });
    }
    output << '}';
    output << ",\"gpuEngine\":{\"available\":" << (gpu.valid && gpu.instances > 0u ? "true" : "false") << ",\"instances\":" << gpu.instances
           << ",\"newInstances\":" << gpu.newInstances << ",\"resets\":" << gpu.resets << ",\"runningTime100ns\":" << gpu.total100ns
           << ",\"runningTime3d100ns\":" << gpu.threeD100ns << '}';
    output << ",\"otherGpuBusy\":";
    if (otherGpuBusy >= 0.0)
        output << otherGpuBusy;
    else
        output << "null";
    if (wrappers)
        output << ",\"wrappers\":{\"wrappedContexts\":" << wrappers->direct2D.wrappedContexts << ",\"writeFactories\":" << wrappers->writeFactoryCount
               << ",\"escapedInterfaces\":" << wrappers->EscapedInterfaces() << ",\"unwrappedTargets\":" << wrappers->direct2D.unwrappedTargets << '}';
    if (sampler)
    {
        output << ",\"sampling\":{\"intervalMicroseconds\":" << kSampleIntervalMicroseconds << ",\"wakes\":" << sampler->wakes
               << ",\"outside\":" << sampler->outsideTicks << ",\"failed\":" << sampler->failedSamples << ",\"unknown\":" << sampler->unknownSamples
               << ",\"modules\":[";
        bool first        = true;
        const auto& names = sampler->Map().Modules();
        for (size_t index = 0u; index < sampler->Tallies().size(); ++index)
        {
            const Sampler::Tally& tally = sampler->Tallies()[index];
            if (tally.running[kPrepare] + tally.running[kComposite] + tally.waiting[kPrepare] + tally.waiting[kComposite] == 0u)
                continue;
            const bool known = index < names.size();
            output << (first ? "" : ",") << "{\"module\":" << FrameTiming::JsonString(known ? names[index].name : std::string("(no module)")) << ",\"class\":\""
                   << ModuleClassName(known ? names[index].moduleClass : ModuleClass::Other) << "\",\"prepareRunning\":" << tally.running[kPrepare]
                   << ",\"compositeRunning\":" << tally.running[kComposite] << ",\"prepareWaiting\":" << tally.waiting[kPrepare]
                   << ",\"compositeWaiting\":" << tally.waiting[kComposite] << '}';
            first = false;
        }
        output << "]}";
    }
    output << ",\"timing\":";
    FrameTiming::Write(run, output);
    output << "}\n";
}

struct Result
{
    FrameTiming::Run run;
    Clock begin, end;
    GpuRunningTime::Reading gpu;
    double otherGpuBusy = -1.0;
};

// One pass on any frame timing fixture (another test's own fixture, for example a W4.0 spike that paints, replays or draws a
// tile): `inner` must not have created its host yet, since the pass installs the counting wrappers before its Initialize.
// `frame` labels the report. The report (WriteReport) goes to `output`.
inline void RunPass(Pass pass, std::unique_ptr<FrameTiming::Fixture> inner, std::wstring_view frame, const FrameTiming::Options& options, std::ostream& output)
{
    Check(inner != nullptr, "frame attribution has a fixture to measure");
    // The wrappers must exist before the fixture's host creates its Direct2D and DirectWrite factories (Initialize), and outlive
    // the host, which releases the wrapped objects when the fixture is destroyed.
    std::optional<NativeWorkTests::CountingWrappers> wrappers;
    std::atomic<uint32_t> phase{kOutside};
    InstrumentedFixture fixture(std::move(inner), pass, options.warmupFrames, phase);
    fixture.records_.reserve(FrameTiming::kMaximumFrames);
    NativeWorkTests::CallTicks* const previousTicks = NativeWorkTests::g_callTicks;
    const auto restore                              = wil::scope_exit([&] { NativeWorkTests::g_callTicks = previousTicks; });
    if (pass == Pass::Phases)
    {
        wrappers.emplace();
        NativeWorkTests::g_callTicks = &fixture.ticks_;
    }
    std::optional<Sampler> sampler;
    if (pass == Pass::Sampling)
        sampler.emplace(phase);
    Result result;
    {
        // The sampler starts on the measuring thread before Measure pins it; it samples only inside preparation and composition.
        if (sampler)
        {
            sampler->measuringProcessor = &fixture.measuringProcessor_;
            sampler->Start();
        }
        result.begin        = ReadClock();
        result.run          = FrameTiming::Measure(fixture, options);
        result.end          = ReadClock();
        result.gpu          = fixture.MeasuredGpuRunningTime();
        result.otherGpuBusy = fixture.MeasuredOtherGpuBusy();
        if (sampler)
            sampler->Stop();
    }
    WriteReport(output,
                pass,
                frame,
                result.run,
                fixture,
                result.begin,
                result.end,
                result.gpu,
                result.otherGpuBusy,
                wrappers ? &*wrappers : nullptr,
                sampler ? &*sampler : nullptr);
}

// One pass on a catalog fixture as FX<N>[@variant] with dirty frames `frame` (MakeAttributionFixture).
inline void RunPass(Pass pass, std::wstring_view fixtureName, std::wstring_view frame, const FrameTiming::Options& options, std::ostream& output)
{
    auto inner = PerformanceFixtures::MakeAttributionFixture(fixtureName, frame);
    Check(inner != nullptr, "frame attribution names a built embedded catalog fixture and a frame it has");
    RunPass(pass, std::move(inner), frame, options, output);
}

// DxUi.EmbeddedTests.exe --frame-attribution <output> <fixture> <full|timed|phase:NAME> Hardware|Warp <auto|none|index> <timing|phases|sampling>
//   <minFrames> <minMilliseconds>
inline int RunCommand(int argc, wchar_t** argv)
{
    FrameTiming::Options options;
    size_t milliseconds = 0u;
    const std::wstring_view passText(argc == 10 ? argv[7] : L"");
    const bool knownPass = passText == L"timing" || passText == L"phases" || passText == L"sampling";
    if (argc != 10 || (std::wstring_view(argv[5]) != L"Hardware" && std::wstring_view(argv[5]) != L"Warp") || ! knownPass ||
        ! FrameTiming::TryParseCount(argv[8], options.minimumFrames) || ! FrameTiming::TryParseCount(argv[9], milliseconds) || options.minimumFrames == 0u)
    {
        std::fputs("Frame attribution requires output, fixture, full|timed|phase:<name>, Hardware|Warp, auto|none|processor, timing|phases|sampling, "
                   "minimum frames and minimum milliseconds.\n",
                   stderr);
        return 2;
    }
    if (FrameTiming::SkipsFixture(argv[3]))
        return 0;
    if (! PerformanceFixtures::MakeAttributionFixture(argv[3], argv[4]))
    {
        std::fputs("Unknown frame attribution fixture or frame.\n", stderr);
        return 2;
    }
    const Pass pass        = passText == L"timing" ? Pass::Timing : passText == L"phases" ? Pass::Phases : Pass::Sampling;
    options.phase          = FrameTiming::Phase::Dirty;
    options.hardware       = std::wstring_view(argv[5]) == L"Hardware";
    options.pinCore        = argv[6];
    options.minimumSeconds = static_cast<double>(milliseconds) / 1000.0;
    std::ofstream output{std::filesystem::path(argv[2])};
    Check(bool(output), "frame attribution output file");
    RunPass(pass, argv[3], argv[4], options, output);
    output.close();
    Check(bool(output), "frame attribution report written");
    return 0;
}

// Deterministic WARP check of the method's mechanics, not of any timing value: the module classes, every pass on FX5 with a
// full frame and with its timed frame, wrapped-call time inside preparation, the counters of a full frame and samples
// attributed to loaded modules.
inline void SelfTest()
{
    Check(ClassifyModule(L"d2d1.dll", L"c:\\windows\\system32\\d2d1.dll", false) == ModuleClass::D2d1 &&
              ClassifyModule(L"textshaping.dll", L"c:\\windows\\system32\\textshaping.dll", false) == ModuleClass::DWrite &&
              ClassifyModule(L"nvwgf2umx.dll", L"c:\\windows\\system32\\driverstore\\filerepository\\nv\\nvwgf2umx.dll", false) == ModuleClass::D3d11Driver &&
              ClassifyModule(L"dxgi.dll", L"c:\\windows\\system32\\dxgi.dll", false) == ModuleClass::D3d11Driver &&
              ClassifyModule(L"ntdll.dll", L"c:\\windows\\system32\\ntdll.dll", false) == ModuleClass::System &&
              ClassifyModule(L"ucrtbase.dll", L"c:\\windows\\system32\\ucrtbase.dll", false) == ModuleClass::Crt &&
              ClassifyModule(L"anything.exe", L"c:\\anything.exe", true) == ModuleClass::DxUi &&
              ClassifyModule(L"user32.dll", L"c:\\windows\\system32\\user32.dll", false) == ModuleClass::Other,
          "frame attribution classifies modules");
    // GPU engine running time between snapshots: per instance, by name; instances present in one snapshot only are left out
    // unless counted whole on request; other processes' busy time per engine of one adapter.
    {
        using Engine      = NativeWorkTests::GpuEngineTime;
        const auto parsed = Engine::Parse(L"pid_42_luid_0x00000000_0x0000D1B2_phys_0_eng_3_engtype_3D");
        Check(parsed.valid && parsed.pid == 42u && parsed.luid == L"0x00000000_0x0000D1B2" && parsed.type == L"3D" &&
                  parsed.engine == L"luid_0x00000000_0x0000D1B2_phys_0_eng_3_engtype_3D" && ! Engine::Parse(L"luid_0x0_0x1_phys_0").valid,
              "GPU engine instance names parse");
        const auto snapshot = [](std::initializer_list<Engine::Instance> values)
        {
            Engine::Reading reading;
            reading.valid  = true;
            reading.values = values;
            std::sort(reading.values.begin(), reading.values.end(), [](const auto& left, const auto& right) { return left.name < right.name; });
            return reading;
        };
        const Engine::Reading before  = snapshot({{L"pid_1_luid_0x0_0x1_phys_0_eng_0_engtype_3D", 100u},
                                                  {L"pid_2_luid_0x0_0x1_phys_0_eng_0_engtype_3D", 50u},
                                                  {L"pid_3_luid_0x0_0x1_phys_0_eng_0_engtype_3D", 70u},
                                                  {L"pid_2_luid_0x0_0x2_phys_0_eng_0_engtype_3D", 10u}});
        const Engine::Reading after   = snapshot({{L"pid_1_luid_0x0_0x1_phys_0_eng_0_engtype_3D", 160u},
                                                  {L"pid_2_luid_0x0_0x1_phys_0_eng_0_engtype_3D", 80u},
                                                  {L"pid_4_luid_0x0_0x1_phys_0_eng_0_engtype_3D", 900u},
                                                  {L"pid_2_luid_0x0_0x2_phys_0_eng_0_engtype_3D", 15u}});
        const Engine::Reading dropped = Engine::Delta(before, after);
        Check(dropped.total100ns == 60u + 30u + 5u && dropped.newInstances == 1u && dropped.goneInstances == 1u, "a new or gone instance is left out");
        Check(Engine::Delta(before, after, Engine::NewInstances::CountWhole).total100ns == 60u + 30u + 5u + 900u, "a new instance counted whole on request");
        Engine::Reading counts;
        const DWORD excluded[]{3u};
        const auto engines = Engine::DeltaByEngine(before, after, L"0x0_0x1", 1u, excluded, &counts);
        Check(engines.size() == 1u && engines[0].own100ns == 60u && engines[0].others100ns == 30u && engines[0].type == L"3D" && counts.newInstances == 1u,
              "other processes' busy time on one adapter's engine");
    }
    ModuleMap map;
    map.Refresh();
    const size_t self = map.Find(reinterpret_cast<uintptr_t>(&SelfTest));
    Check(self < map.Modules().size() && map.Modules()[self].moduleClass == ModuleClass::DxUi, "frame attribution finds this executable's code");
    Check(map.Find(0u) == map.Modules().size(), "frame attribution finds no module at address 0");
    Check(PerformanceFixtures::MakeAttributionFixture(L"FX5", L"full") && PerformanceFixtures::MakeAttributionFixture(L"FX12", L"phase:hover") &&
              PerformanceFixtures::MakeAttributionFixture(L"FX3@fhd", L"timed") && ! PerformanceFixtures::MakeAttributionFixture(L"FX5", L"phase:missing") &&
              ! PerformanceFixtures::MakeAttributionFixture(L"FX5", L"whole"),
          "frame attribution resolves fixtures and frames");

    FrameTiming::Options options;
    options.hardware       = false;
    options.pinCore        = L"none";
    options.warmupFrames   = 2u;
    options.minimumFrames  = 8u;
    options.minimumSeconds = 0.01;
    for (const std::wstring_view frame : {std::wstring_view(L"full"), std::wstring_view(L"timed")})
        for (const Pass pass : {Pass::Timing, Pass::Phases, Pass::Sampling})
        {
            std::ostringstream report;
            RunPass(pass, L"FX5", frame, options, report);
            const std::string text = report.str();
            Check(text.find("\"method\":\"dxui-frame-attribution-v1\"") != std::string::npos &&
                      text.find("\"method\":\"dxui-frame-timing-v1\"") != std::string::npos && text.back() == '\n',
                  "frame attribution report is complete");
        }
    // Another test's own fixture object, here a catalog fixture built by hand: the overload a spike uses.
    {
        std::ostringstream report;
        RunPass(Pass::Phases, PerformanceFixtures::MakeAttributionFixture(L"FX5", L"timed"), L"spike frame", options, report);
        const std::string text = report.str();
        Check(text.find("\"pass\":\"phases\"") != std::string::npos && text.find("\"frame\":\"spike frame\"") != std::string::npos &&
                  text.find("\"prepareEndDrawTicks\"") != std::string::npos,
              "frame attribution measures a fixture object handed to it");
    }
    // The phases pass: every measured full frame prepares once, paints the whole tree, issues draw calls and spends time inside
    // EndDraw and the draw calls, all inside its preparation.
    {
        auto inner = PerformanceFixtures::MakeAttributionFixture(L"FX5", L"full");
        NativeWorkTests::CountingWrappers wrappers; // outlives the fixture's host, which releases the wrapped objects
        std::atomic<uint32_t> phase{kOutside};
        InstrumentedFixture fixture(std::move(inner), Pass::Phases, options.warmupFrames, phase);
        NativeWorkTests::g_callTicks = &fixture.ticks_;
        const auto restore           = wil::scope_exit([] { NativeWorkTests::g_callTicks = nullptr; });
        const auto run               = FrameTiming::Measure(fixture, options);
        Check(fixture.records_.size() == run.frames, "frame attribution records every measured frame");
        for (const FrameRecord& record : fixture.records_)
        {
            const auto& calls      = record.prepareCalls;
            const uint64_t wrapped = calls.beginDraw + calls.endDraw + calls.flush + calls.drawCalls + calls.textLayouts + calls.textFormats;
            Check(record.preparations == 1u && record.paintExecutions > 1u && record.drawCalls > 0u, "a full FX5 frame prepares once and paints the tree");
            Check(calls.endDraw > 0u && calls.drawCalls > 0u && wrapped <= record.prepareTicks, "wrapped-call time is inside preparation");
        }
    }
    // The sampling pass attributes samples to loaded modules and only inside a frame's measured work.
    {
        auto inner = PerformanceFixtures::MakeAttributionFixture(L"FX5", L"full");
        std::atomic<uint32_t> phase{kOutside};
        InstrumentedFixture fixture(std::move(inner), Pass::Sampling, options.warmupFrames, phase);
        Sampler sampler(phase);
        sampler.Start();
        FrameTiming::Options longer = options;
        longer.minimumSeconds       = 0.3;
        static_cast<void>(FrameTiming::Measure(fixture, longer));
        sampler.Stop();
        uint64_t inside = 0u;
        for (const Sampler::Tally& tally : sampler.Tallies())
            inside += tally.running[kPrepare] + tally.running[kComposite] + tally.waiting[kPrepare] + tally.waiting[kComposite];
        Check(sampler.wakes > 0u && inside + sampler.outsideTicks + sampler.failedSamples <= sampler.wakes, "every sample is counted once");
        Check(inside > 0u, "the sampler takes samples inside preparation and composition");
        Check(sampler.Tallies().size() == sampler.Map().Modules().size() + 1u, "one tally per loaded module and one for no module");
    }
}
} // namespace FrameAttribution
#pragma warning(pop)
