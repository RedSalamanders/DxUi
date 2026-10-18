#pragma once

#include "Embedded.Tests.ComplexUiBenchmark.h"
#include <charconv>
#include <memory>
#include <sstream>
#include <tlhelp32.h>

// Opt-in fixture only. OS/heap sampling and output never occur in a frame window. The fixed
// extra warmup and instrumentation make these observations diagnostic, not acceptance metrics.
namespace ComplexUiProfiling
{
inline constexpr size_t kThreadLimit     = 256u;
inline constexpr size_t kThreadScanLimit = 65536u;
inline constexpr size_t kStageCount      = 12u;
using Clock                              = std::chrono::steady_clock;

inline uint64_t FileTimeValue(const FILETIME& value) noexcept
{
    return (uint64_t{value.dwHighDateTime} << 32u) | value.dwLowDateTime;
}

struct CpuSample
{
    uint64_t kernel100ns = 0u, user100ns = 0u, cycles = 0u;
    DWORD timesError = ERROR_INVALID_HANDLE, cyclesError = ERROR_INVALID_HANDLE;
    bool timesAvailable = false, cyclesAvailable = false;
};

inline DWORD QueryError() noexcept
{
    const DWORD error = GetLastError();
    return error != ERROR_SUCCESS ? error : ERROR_GEN_FAILURE;
}

inline CpuSample SampleCpu(HANDLE handle, bool process) noexcept
{
    CpuSample sample;
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (! (process ? GetProcessTimes(handle, &creation, &exit, &kernel, &user) : GetThreadTimes(handle, &creation, &exit, &kernel, &user)))
        sample.timesError = QueryError();
    else
    {
        sample.kernel100ns    = FileTimeValue(kernel);
        sample.user100ns      = FileTimeValue(user);
        sample.timesAvailable = true;
        sample.timesError     = ERROR_SUCCESS;
    }
    if (process ? QueryProcessCycleTime(handle, &sample.cycles) : QueryThreadCycleTime(handle, &sample.cycles))
    {
        sample.cyclesAvailable = true;
        sample.cyclesError     = ERROR_SUCCESS;
    }
    else
    {
        sample.cycles      = 0u;
        sample.cyclesError = QueryError();
    }
    return sample;
}

struct CpuCounters
{
    CpuSample main, process;
};

inline CpuCounters ReadCpu() noexcept
{
    // Separately read counters are not a simultaneous scheduler trace. Keep both originals;
    // process-minus-main deltas also include transient/unobserved worker threads.
    return {SampleCpu(GetCurrentThread(), false), SampleCpu(GetCurrentProcess(), true)};
}

struct ThreadSample
{
    // handleError also describes a failed/mismatched owner check; unqueried telemetry
    // stays explicitly unavailable if a snapshot TID has retired or been reused.
    DWORD id = 0u, handleError = ERROR_SUCCESS, creationError = ERROR_INVALID_HANDLE;
    DWORD ownerProcessId = 0u, ownerError = ERROR_INVALID_HANDLE;
    bool ownerAvailable = false;
    DWORD priorityError = ERROR_INVALID_HANDLE, affinityError = ERROR_INVALID_HANDLE;
    bool creationAvailable = false, priorityAvailable = false, affinityAvailable = false;
    uint64_t creation100ns = 0u;
    CpuSample cpu;
    GROUP_AFFINITY affinity{};
    int priority = 0;
};

struct Stage
{
    const char* name       = nullptr;
    size_t completedFrames = 0u;
    double elapsedMs = 0.0, samplingMs = 0.0, heapSamplingMs = 0.0;
    PROCESS_MEMORY_COUNTERS_EX memory{};
    CpuCounters cpu;
    DWORD handles = 0u, priority = 0u, threadError = ERROR_SUCCESS;
    DWORD handlesError = ERROR_SUCCESS, priorityError = ERROR_SUCCESS, affinityError = ERROR_SUCCESS, groupsError = ERROR_SUCCESS;
    bool handlesAvailable = false, priorityAvailable = false, affinityAvailable = false, groupsAvailable = false;
    DWORD_PTR processMask = 0u, systemMask = 0u;
    std::array<USHORT, 64> groups{};
    USHORT groupCount = 0u;
    std::array<ThreadSample, kThreadLimit> threads{};
    size_t threadCount = 0u, threadsScanned = 0u;
    bool gridAvailable = false, viewAvailable = false, heapComplete = true;
    DxUi::Grid::GridDebugTextLayoutStatistics grid{};
    DxUi::EmbeddedStatistics view{};
    DWORD heapCountHint = 0u, heapInventoryError = ERROR_SUCCESS;
    std::string heapJson;
};

inline void SampleThreads(Stage& stage)
{
    const HANDLE raw = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0u);
    if (raw == INVALID_HANDLE_VALUE)
    {
        stage.threadError = QueryError();
        return;
    }
    const wil::unique_handle snapshot(raw);
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (! Thread32First(snapshot.get(), &entry))
    {
        stage.threadError = QueryError();
        return;
    }
    do
    {
        if (stage.threadsScanned == kThreadScanLimit)
        {
            stage.threadError = ERROR_MORE_DATA;
            break;
        }
        ++stage.threadsScanned;
        if (entry.th32OwnerProcessID != GetCurrentProcessId())
            continue;
        if (stage.threadCount == stage.threads.size())
        {
            stage.threadError = ERROR_MORE_DATA;
            break;
        }
        auto& sample = stage.threads[stage.threadCount++];
        sample.id    = entry.th32ThreadID;
        const wil::unique_handle thread(OpenThread(THREAD_QUERY_INFORMATION, FALSE, sample.id));
        if (! thread)
        {
            sample.handleError = QueryError();
            continue;
        }
        sample.ownerProcessId = GetProcessIdOfThread(thread.get());
        if (sample.ownerProcessId == 0u)
        {
            sample.ownerError  = QueryError();
            sample.handleError = sample.ownerError;
            continue;
        }
        sample.ownerAvailable = true;
        sample.ownerError     = ERROR_SUCCESS;
        if (sample.ownerProcessId != GetCurrentProcessId())
        {
            sample.handleError = ERROR_INVALID_OWNER;
            continue;
        }
        FILETIME creation{}, exit{}, kernel{}, user{};
        if (! GetThreadTimes(thread.get(), &creation, &exit, &kernel, &user))
        {
            sample.creationError = QueryError();
        }
        else
        {
            sample.creationAvailable = true;
            sample.creationError     = ERROR_SUCCESS;
            sample.creation100ns     = FileTimeValue(creation);
        }
        sample.cpu      = SampleCpu(thread.get(), false);
        sample.priority = GetThreadPriority(thread.get());
        if (sample.priority == THREAD_PRIORITY_ERROR_RETURN)
        {
            sample.priority      = 0;
            sample.priorityError = QueryError();
        }
        else
        {
            sample.priorityAvailable = true;
            sample.priorityError     = ERROR_SUCCESS;
        }
        if (! GetThreadGroupAffinity(thread.get(), &sample.affinity))
        {
            sample.affinity      = {};
            sample.affinityError = QueryError();
        }
        else
        {
            sample.affinityAvailable = true;
            sample.affinityError     = ERROR_SUCCESS;
        }
    } while (Thread32Next(snapshot.get(), &entry));
    if (stage.threadError == ERROR_SUCCESS)
    {
        const DWORD error = GetLastError();
        if (error != ERROR_NO_MORE_FILES)
            stage.threadError = error != ERROR_SUCCESS ? error : ERROR_GEN_FAILURE;
    }
}

inline Stage Capture(const char* name, size_t completedFrames, Clock::time_point started, DxUi::Grid* grid, const DxUi::EmbeddedHost* view, bool heaps)
{
    const auto before = Clock::now();
    Stage stage;
    stage.name              = name;
    stage.completedFrames   = completedFrames;
    stage.elapsedMs         = std::chrono::duration<double, std::milli>(before - started).count();
    stage.cpu               = ReadCpu();
    stage.memory            = ComplexUiBenchmark::Memory();
    stage.priority          = GetPriorityClass(GetCurrentProcess());
    stage.priorityAvailable = stage.priority != 0u;
    stage.priorityError     = stage.priorityAvailable ? ERROR_SUCCESS : QueryError();
    stage.handlesAvailable  = GetProcessHandleCount(GetCurrentProcess(), &stage.handles) != FALSE;
    stage.handlesError      = stage.handlesAvailable ? ERROR_SUCCESS : QueryError();
    if (! stage.handlesAvailable)
        stage.handles = 0u;
    stage.affinityAvailable = GetProcessAffinityMask(GetCurrentProcess(), &stage.processMask, &stage.systemMask) != FALSE;
    stage.affinityError     = stage.affinityAvailable ? ERROR_SUCCESS : QueryError();
    if (! stage.affinityAvailable)
        stage.processMask = stage.systemMask = 0u;
    stage.groupCount      = static_cast<USHORT>(stage.groups.size());
    stage.groupsAvailable = GetProcessGroupAffinity(GetCurrentProcess(), &stage.groupCount, stage.groups.data()) != FALSE;
    stage.groupsError     = stage.groupsAvailable ? ERROR_SUCCESS : QueryError();
    if (! stage.groupsAvailable)
        stage.groupCount = 0u;
    SampleThreads(stage);
    if (grid)
    {
        stage.gridAvailable = true;
        stage.grid          = grid->DebugGetTextLayoutStatistics();
    }
    if (view)
    {
        stage.viewAvailable = true;
        stage.view          = view->GetStatistics();
    }
    if (heaps)
    {
        const auto heapStarted = Clock::now();
        std::ostringstream heapOutput;
        stage.heapCountHint = GetProcessHeaps(0u, nullptr);
        if (stage.heapCountHint == 0u || stage.heapCountHint > 128u)
        {
            stage.heapInventoryError = stage.heapCountHint == 0u ? QueryError() : ERROR_MORE_DATA;
            stage.heapComplete       = false;
        }
        else
            DxUiTestSupport::WriteHeapDiagnostic(heapOutput,
                                                 [&](bool ok, const char*)
            {
                if (! ok)
                {
                    stage.heapInventoryError = ERROR_INVALID_DATA;
                    stage.heapComplete       = false;
                }
            });
        stage.heapJson = heapOutput.str();
        // The shared helper guards enumeration before walking and reports individual walk/lock
        // failures in its fixed JSON fragment. Propagate those errors without a second heap walk.
        size_t position = 0u, sampledHeaps = 0u;
        while ((position = stage.heapJson.find("\"error\":", position)) != std::string::npos)
        {
            position += 8u;
            DWORD error       = ERROR_INVALID_DATA;
            const auto parsed = std::from_chars(stage.heapJson.data() + position, stage.heapJson.data() + stage.heapJson.size(), error);
            if (parsed.ec != std::errc{} || parsed.ptr == stage.heapJson.data() + stage.heapJson.size() || *parsed.ptr != '}' || error != ERROR_SUCCESS)
                stage.heapComplete = false;
            ++sampledHeaps;
        }
        if (sampledHeaps == 0u || sampledHeaps > 128u)
            stage.heapComplete = false;
        if (stage.heapJson.empty())
            stage.heapJson = ",\"heaps\":[]";
        stage.heapSamplingMs = std::chrono::duration<double, std::milli>(Clock::now() - heapStarted).count();
        Check(stage.heapJson.size() <= 65536u, "profiling heap output fits fixed report budget");
    }
    stage.samplingMs = std::chrono::duration<double, std::milli>(Clock::now() - before).count();
    return stage;
}

struct Frame
{
    double completedMs = 0.0, prepareMs = 0.0, compositeMs = 0.0;
    size_t cppAllocations = 0u, compositeAllocations = 0u;
};

struct Window
{
    const char* name   = nullptr;
    size_t firstUpdate = 0u, frameCount = 0u;
    bool dirty    = false;
    double wallMs = 0.0;
    CpuCounters before, after;
    std::array<Frame, 100> frames{};
};

inline void WriteCpu(std::ostream& output, const CpuSample& cpu)
{
    output << "{\"timesAvailable\":" << (cpu.timesAvailable ? "true" : "false") << ",\"timesError\":" << cpu.timesError
           << ",\"cyclesAvailable\":" << (cpu.cyclesAvailable ? "true" : "false") << ",\"cyclesError\":" << cpu.cyclesError
           << ",\"kernel100ns\":" << cpu.kernel100ns << ",\"user100ns\":" << cpu.user100ns << ",\"cycles\":" << cpu.cycles << '}';
}

inline void WriteCounters(std::ostream& output, const CpuCounters& counters)
{
    output << "{\"main\":";
    WriteCpu(output, counters.main);
    output << ",\"process\":";
    WriteCpu(output, counters.process);
    output << '}';
}

inline void WriteMask(std::ostream& output, uint64_t mask)
{
    const auto flags = output.flags();
    const auto fill  = output.fill();
    output << '"' << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << mask << '"';
    output.flags(flags);
    output.fill(fill);
}

inline void WriteStage(std::ostream& output, const Stage& stage, bool heaps)
{
    output << "{\"stage\":\"" << stage.name << "\",\"completedFrames\":" << stage.completedFrames << ",\"elapsedMs\":" << stage.elapsedMs
           << ",\"samplingMs\":" << stage.samplingMs << ",\"cpu\":";
    WriteCounters(output, stage.cpu);
    output << ",\"process\":{\"privateBytes\":" << stage.memory.PrivateUsage << ",\"workingSetBytes\":" << stage.memory.WorkingSetSize
           << ",\"peakWorkingSetBytes\":" << stage.memory.PeakWorkingSetSize << ",\"handles\":" << stage.handles
           << ",\"handlesAvailable\":" << (stage.handlesAvailable ? "true" : "false") << ",\"handlesError\":" << stage.handlesError
           << "},\"scheduling\":{\"priorityAvailable\":" << (stage.priorityAvailable ? "true" : "false") << ",\"priorityError\":" << stage.priorityError
           << ",\"priorityClass\":" << stage.priority << ",\"affinityAvailable\":" << (stage.affinityAvailable ? "true" : "false")
           << ",\"affinityError\":" << stage.affinityError << ",\"groupsAvailable\":" << (stage.groupsAvailable ? "true" : "false")
           << ",\"groupsError\":" << stage.groupsError << ",\"processMask\":";
    WriteMask(output, stage.processMask);
    output << ",\"systemMask\":";
    WriteMask(output, stage.systemMask);
    output << ",\"groups\":[";
    for (USHORT index = 0u; index < stage.groupCount && index < stage.groups.size(); ++index)
    {
        if (index)
            output << ',';
        output << stage.groups[index];
    }
    output << "],\"threadLimit\":" << kThreadLimit << ",\"threadScanLimit\":" << kThreadScanLimit << ",\"threadsScanned\":" << stage.threadsScanned
           << ",\"threadError\":" << stage.threadError << ",\"threads\":[";
    for (size_t index = 0u; index < stage.threadCount; ++index)
    {
        if (index)
            output << ',';
        const auto& thread = stage.threads[index];
        output << "{\"id\":" << thread.id << ",\"creation100ns\":" << thread.creation100ns << ",\"handleError\":" << thread.handleError
               << ",\"ownerProcessId\":" << thread.ownerProcessId << ",\"ownerAvailable\":" << (thread.ownerAvailable ? "true" : "false")
               << ",\"ownerError\":" << thread.ownerError << ",\"creationAvailable\":" << (thread.creationAvailable ? "true" : "false")
               << ",\"creationError\":" << thread.creationError << ",\"priorityAvailable\":" << (thread.priorityAvailable ? "true" : "false")
               << ",\"priorityError\":" << thread.priorityError << ",\"affinityAvailable\":" << (thread.affinityAvailable ? "true" : "false")
               << ",\"affinityError\":" << thread.affinityError << ",\"priority\":" << thread.priority << ",\"group\":" << thread.affinity.Group
               << ",\"mask\":";
        WriteMask(output, thread.affinity.Mask);
        output << ",\"cpu\":";
        WriteCpu(output, thread.cpu);
        output << '}';
    }
    output << "]},\"grid\":{\"available\":" << (stage.gridAvailable ? "true" : "false") << ",\"lookups\":" << stage.grid.lookups
           << ",\"hits\":" << stage.grid.hits << ",\"layoutCreations\":" << stage.grid.layoutCreations << ",\"shapedUnits\":" << stage.grid.shapedUnits
           << ",\"retainedLayouts\":" << stage.grid.retainedLayouts << ",\"capacity\":" << stage.grid.capacity
           << ",\"displayLayouts\":" << stage.grid.displayLayouts << ",\"displayCapacity\":" << stage.grid.displayCapacity
           << ",\"textUnits\":" << stage.grid.textUnits << ",\"tableBytes\":" << stage.grid.tableBytes
           << "},\"view\":{\"available\":" << (stage.viewAvailable ? "true" : "false") << ",\"preparations\":" << stage.view.preparations
           << ",\"composites\":" << stage.view.composites << ",\"surfaceAllocations\":" << stage.view.surfaceAllocations
           << ",\"surfaceBytes\":" << stage.view.surfaceBytes << ",\"replacementPeakBytes\":" << stage.view.replacementPeakBytes
           << "},\"heapSampler\":{\"enabled\":" << (heaps ? "true" : "false") << ",\"complete\":" << (stage.heapComplete ? "true" : "false")
           << ",\"heapLimit\":128,\"durationMs\":" << stage.heapSamplingMs << ",\"inventoryError\":" << stage.heapInventoryError
           << ",\"heapCountHint\":" << stage.heapCountHint << ",\"jsonBytes\":" << stage.heapJson.size()
           << ",\"jsonCapacityBytes\":" << stage.heapJson.capacity() << stage.heapJson << "}}";
}

inline bool CpuComplete(const CpuSample& cpu) noexcept
{
    return cpu.timesAvailable && cpu.cyclesAvailable;
}

inline bool StageComplete(const Stage& stage, bool heaps) noexcept
{
    if (! CpuComplete(stage.cpu.main) || ! CpuComplete(stage.cpu.process) || ! stage.handlesAvailable || ! stage.priorityAvailable ||
        ! stage.affinityAvailable || ! stage.groupsAvailable || stage.threadError != ERROR_SUCCESS || (heaps && ! stage.heapComplete))
        return false;
    for (size_t index = 0u; index < stage.threadCount; ++index)
    {
        const auto& thread = stage.threads[index];
        if (thread.handleError != ERROR_SUCCESS || ! thread.creationAvailable || ! thread.priorityAvailable || ! thread.affinityAvailable ||
            ! CpuComplete(thread.cpu))
            return false;
    }
    return true;
}

__declspec(noinline) inline void Run(const wchar_t* outputPath, bool multiline, bool heaps)
{
    const auto started = Clock::now();
    const auto stages  = std::make_unique<std::array<Stage, kStageCount>>();
    std::array<Window, 5> windows{};
    size_t stageIndex      = 0u;
    size_t completedFrames = 0u;
    const auto capture     = [&](const char* name, DxUi::Grid* grid, const DxUi::EmbeddedHost* view = nullptr)
    {
        Check(stageIndex < stages->size(), "profiling stage inventory fits bounded storage");
        (*stages)[stageIndex++] = Capture(name, completedFrames, started, grid, view, heaps);
    };
    capture("entry", nullptr);
    {
        GraphicsFixture gpu;
        gpu.width  = 1280u;
        gpu.height = 720u;
        Hr(gpu.Create(), "profiling WARP device and target");
        capture("device-and-target", nullptr);
        ComplexUiScene scene;
        scene.model.multilineGrid = multiline;
        if (multiline)
            for (size_t index = 0u; index < scene.model.names.size(); ++index)
                scene.model.names[index] = L"Description française détaillée de l’élément " + std::to_wstring(index) +
                                           L" : vérifier les informations avant de poursuivre.\nUne deuxième phrase complète avec é et 📷.";
        Hr(scene.Initialize(gpu.device.get()), "profiling independent synthetic scene");
        if (multiline)
        {
            scene.grid->SetRowHeightDip(64.0f);
            scene.grid->SetLineClamp(2u);
        }
        capture("scene", scene.grid, &scene.view);
        D3D11_TEXTURE2D_DESC descriptor{};
        descriptor.Width = descriptor.Height = descriptor.MipLevels = descriptor.ArraySize = descriptor.SampleDesc.Count = 1u;
        descriptor.Format                                                                                                = DXGI_FORMAT_B8G8R8A8_UNORM;
        descriptor.Usage                                                                                                 = D3D11_USAGE_STAGING;
        descriptor.CPUAccessFlags                                                                                        = D3D11_CPU_ACCESS_READ;
        wil::com_ptr_nothrow<ID3D11Texture2D> completion;
        Hr(gpu.device->CreateTexture2D(&descriptor, nullptr, completion.put()), "profiling one-pixel completion texture");
        const auto complete = [&]
        {
            const D3D11_BOX pixel{0u, 0u, 0u, 1u, 1u, 1u};
            gpu.context->CopySubresourceRegion(completion.get(), 0u, 0u, 0u, 0u, gpu.target.get(), 0u, &pixel);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            Hr(gpu.context->Map(completion.get(), 0u, D3D11_MAP_READ, 0u, &mapped), "profiling completed frame");
            gpu.context->Unmap(completion.get(), 0u);
        };
        const auto runWindow = [&](Window& window, const char* name, size_t firstUpdate, size_t count, bool dirty)
        {
            window.name              = name;
            window.firstUpdate       = firstUpdate;
            window.frameCount        = count;
            window.dirty             = dirty;
            window.before            = ReadCpu();
            const auto before        = scene.view.GetStatistics();
            const auto windowStarted = Clock::now();
            for (size_t index = 0u; index < count; ++index)
            {
                auto& frame             = window.frames[index];
                allocations             = 0u;
                countAllocations        = true;
                const auto frameStarted = Clock::now();
                if (dirty)
                    scene.Update(firstUpdate + index);
                const auto preparationStarted = Clock::now();
                const HRESULT prepared        = scene.view.Prepare(1280u, 720u);
                const auto preparationEnded   = Clock::now();
                Hr(prepared, "profiling preparation");
                Check(prepared == (dirty ? S_OK : S_FALSE), "profiling clean/dirty preparation result");
                gpu.Bind();
                const auto allocationStart  = allocations;
                const auto compositeStarted = Clock::now();
                Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "profiling composition");
                const auto compositeEnded  = Clock::now();
                frame.compositeAllocations = allocations - allocationStart;
                complete();
                const auto frameEnded = Clock::now();
                countAllocations      = false;
                frame.cppAllocations  = allocations;
                frame.prepareMs       = std::chrono::duration<double, std::milli>(preparationEnded - preparationStarted).count();
                frame.compositeMs     = std::chrono::duration<double, std::milli>(compositeEnded - compositeStarted).count();
                frame.completedMs     = std::chrono::duration<double, std::milli>(frameEnded - frameStarted).count();
            }
            window.wallMs    = std::chrono::duration<double, std::milli>(Clock::now() - windowStarted).count();
            window.after     = ReadCpu();
            const auto after = scene.view.GetStatistics();
            Check(after.surfaceAllocations == before.surfaceAllocations || firstUpdate == 0u, "profiling warm/probe surface reuse");
            Check(after.preparations - before.preparations == (dirty ? count : 0u), "profiling preparation count");
            for (size_t index = 0u; index < count; ++index)
                Check(window.frames[index].compositeAllocations == 0u && (dirty || window.frames[index].cppAllocations == 0u),
                      "profiling allocation invariants");
            completedFrames += count;
        };
        runWindow(windows[0], "warm-1-20", 0u, 20u, true);
        capture("warm20", scene.grid, &scene.view);
        runWindow(windows[1], "warm-21-100", 20u, 80u, true);
        capture("warm100", scene.grid, &scene.view);
        runWindow(windows[2], "warm-101-200", 100u, 100u, true);
        capture("warm200", scene.grid, &scene.view);
        const auto screenshot = std::filesystem::path(outputPath).concat(L".png");
        Hr(gpu.Save(screenshot), "profiling populated synthetic capture outside frame windows");
        capture("capture", scene.grid, &scene.view);
        runWindow(windows[3], "clean-probe", 200u, 40u, false);
        capture("clean-probe", scene.grid, &scene.view);
        runWindow(windows[4], "dirty-probe", 200u, 40u, true);
        capture("dirty-probe", scene.grid, &scene.view);
        scene.view.SetVisible(false);
        const auto hidden = scene.view.GetStatistics();
        Check(scene.view.Prepare(1280u, 720u) == S_FALSE && scene.view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE,
              "profiling hidden view does no work");
        Check(scene.view.GetStatistics().preparations == hidden.preparations && scene.view.GetStatistics().composites == hidden.composites,
              "profiling hidden counters unchanged");
        Check(scene.view.GetStatistics().surfaceBytes == 0u, "profiling hidden view retains no surface");
        capture("hidden", scene.grid, &scene.view);
        scene.view.Controls().SetRoot(nullptr);
        scene.view.Detach();
        completion.reset();
        capture("detached", nullptr, &scene.view);
    }
    capture("released", nullptr);
    Check(stageIndex == kStageCount && completedFrames == 280u, "profiling complete fixed workload inventory");
    bool telemetryComplete = true;
    for (const auto& stage : *stages)
        telemetryComplete = telemetryComplete && StageComplete(stage, heaps);
    for (const auto& window : windows)
        telemetryComplete = telemetryComplete && CpuComplete(window.before.main) && CpuComplete(window.before.process) && CpuComplete(window.after.main) &&
                            CpuComplete(window.after.process);
    LARGE_INTEGER frequency{};
    Check(QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0, "profiling QPC frequency");
    std::ofstream output{std::filesystem::path(outputPath)};
    Check(bool(output), "profiling output file");
    output << std::setprecision(17)
           << "{\"schema\":1,\"fixture\":\"dxui-complex-ui-profiling-v1\",\"classification\":\"diagnostic-only\",\"renderer\":\"WARP\","
           << "\"scenario\":\"" << (multiline ? "MultilineGrid" : "Default") << "\",\"heapSampling\":" << (heaps ? "true" : "false")
           << ",\"compiler\":" << _MSC_FULL_VER << ",\"iteratorDebugLevel\":" << _ITERATOR_DEBUG_LEVEL << ",\"pointerBits\":" << sizeof(void*) * 8u
           << ",\"processId\":" << GetCurrentProcessId() << ",\"mainThreadId\":" << GetCurrentThreadId() << ",\"telemetryCompleteness\":\""
           << (telemetryComplete ? "complete" : "partial") << "\",\"fixedDiagnosticStorageBytes\":" << sizeof(*stages) + sizeof(windows)
           << ",\"width\":1280,\"height\":720,\"dpi\":96,\"controls\":83,\"modelRows\":1000,\"completedFrames\":280,\"hiddenPreparations\":0,"
              "\"hiddenComposites\":0,"
           << "\"clock\":{\"implementation\":\"QueryPerformanceCounter\",\"ticksPerSecond\":" << frequency.QuadPart
           << ",\"tickNanoseconds\":" << 1.0e9 / static_cast<double>(frequency.QuadPart) << "},"
           << "\"timingScope\":\"Diagnostic frame includes mutation, preparation, target bind/clear, composition and blocking one-pixel readback. CPU counters "
              "bracket whole windows; OS/thread/heap sampling and all output are outside windows.\","
           << "\"schedulingScope\":\"Allowed priority/affinity and coarse cumulative CPU times/cycles; main is read before process and live threads are "
              "sampled sequentially. Process-minus-main includes read-order uncertainty and transient threads; no actual processor-placement, context-switch, "
              "call-stack or dedicated-host proof.\","
           << "\"heapScope\":\"128 process-owned heaps; busy/free/committed bytes exclude non-heap virtual allocations. Traversal/lock time is not "
              "independently bounded; the wrapper limits its owned child to 120 seconds. Stage counters include prior sampling overhead.\","
           << "\"storageScope\":\"Entry includes fixed stage/window telemetry storage. Fixed bytes exclude additional string allocations and native "
              "enumeration scratch; heap JSON size/capacity is informational and may overlap fixed string object storage. Thread enumeration scans at most "
              "65536 system records and retains at most 256 owned threads per stage.\","
           << "\"windows\":[";
    for (size_t index = 0u; index < windows.size(); ++index)
    {
        if (index)
            output << ',';
        const auto& window = windows[index];
        output << "{\"window\":\"" << window.name << "\",\"firstUpdate\":" << window.firstUpdate << ",\"dirty\":" << (window.dirty ? "true" : "false")
               << ",\"wallMs\":" << window.wallMs << ",\"cpuBefore\":";
        WriteCounters(output, window.before);
        output << ",\"cpuAfter\":";
        WriteCounters(output, window.after);
        output << ",\"frames\":[";
        for (size_t frameIndex = 0u; frameIndex < window.frameCount; ++frameIndex)
        {
            if (frameIndex)
                output << ',';
            const auto& frame = window.frames[frameIndex];
            output << "{\"completedMs\":" << frame.completedMs << ",\"prepareMs\":" << frame.prepareMs << ",\"compositeMs\":" << frame.compositeMs
                   << ",\"cppAllocations\":" << frame.cppAllocations << ",\"compositeAllocations\":" << frame.compositeAllocations << '}';
        }
        output << "]}";
    }
    output << "],\"stages\":[";
    for (size_t index = 0u; index < stages->size(); ++index)
    {
        if (index)
            output << ',';
        WriteStage(output, (*stages)[index], heaps);
    }
    output << "]}\n";
    output.close();
    Check(bool(output), "profiling complete report written");
}
} // namespace ComplexUiProfiling
