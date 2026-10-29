#pragma once

#include "Embedded.Tests.ComplexUiProfiling.h"
#include <optional>

// This separate diagnostic varies commands in a completed clean WARP frame. It never
// changes the canonical acceptance fixture, and its CPU intervals are not GPU timings.
namespace ComplexUiBoundaries
{
inline constexpr size_t kFrames = 40u;
enum class WarpRasterMode
{
    Default,
    CallingThread
};
enum class Scenario
{
    Default,
    MultilineGrid,
    MultilineGridDistinct
};
inline bool TryParseScenario(std::wstring_view value, Scenario& result) noexcept
{
    if (value == L"Default")
        result = Scenario::Default;
    else if (value == L"MultilineGrid")
        result = Scenario::MultilineGrid;
    else if (value == L"MultilineGridDistinct")
        result = Scenario::MultilineGridDistinct;
    else
        return false;
    return true;
}
inline const char* ScenarioName(Scenario scenario) noexcept
{
    return scenario == Scenario::MultilineGridDistinct ? "MultilineGridDistinct" : scenario == Scenario::MultilineGrid ? "MultilineGrid" : "Default";
}
inline bool TryParsePhase(std::wstring_view value, bool& dirty) noexcept
{
    if (value == L"Clean")
        dirty = false;
    else if (value == L"Dirty")
        dirty = true;
    else
        return false;
    return true;
}
inline void ConfigureModel(ComplexUiModel& model, Scenario scenario)
{
    model.multilineGrid = scenario != Scenario::Default;
    if (scenario == Scenario::MultilineGridDistinct)
        ComplexUiBenchmark::FillDistinctCellTexts(model);
    else if (scenario == Scenario::MultilineGrid)
        for (size_t index = 0u; index < model.names.size(); ++index)
            model.names[index] = L"Description française détaillée de l’élément " + std::to_wstring(index) +
                                 L" : vérifier les informations avant de poursuivre.\nUne deuxième phrase complète avec é et 📷.";
}
inline bool TryParseRasterMode(std::wstring_view value, WarpRasterMode& result) noexcept
{
    if (value == L"Default")
        result = WarpRasterMode::Default;
    else if (value == L"CallingThread")
        result = WarpRasterMode::CallingThread;
    else
        return false;
    return true;
}
inline UINT RequestedDeviceFlags(WarpRasterMode mode) noexcept
{
    return D3D11_CREATE_DEVICE_BGRA_SUPPORT | (mode == WarpRasterMode::CallingThread ? D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS : 0u);
}
inline HRESULT CreateDiagnosticTarget(GraphicsFixture& graphics, WarpRasterMode mode) noexcept
{
    if (mode == WarpRasterMode::Default)
        return graphics.Create(); // Preserve the shared fixture and all canonical device defaults.
    if (mode != WarpRasterMode::CallingThread)
        return E_INVALIDARG;
    // The documented WARP profiling flag moves rasterization onto the calling
    // thread. It changes execution policy; it is not a scheduler-delay timer.
    RETURN_IF_FAILED(D3D11CreateDevice(nullptr,
                                       D3D_DRIVER_TYPE_WARP,
                                       nullptr,
                                       RequestedDeviceFlags(mode),
                                       nullptr,
                                       0,
                                       D3D11_SDK_VERSION,
                                       graphics.device.put(),
                                       nullptr,
                                       graphics.context.put()));
    return graphics.Resize(graphics.width, graphics.height);
}
inline void TestDevicePolicies()
{
    auto scenario = Scenario::Default;
    Check(TryParseScenario(L"MultilineGridDistinct", scenario) && scenario == Scenario::MultilineGridDistinct, "distinct boundary scenario parses");
    Check(! TryParseScenario(L"multilinegriddistinct", scenario) && scenario == Scenario::MultilineGridDistinct,
          "scenario spelling is exact and failure preserves value");
    Check(TryParseScenario(L"MultilineGrid", scenario) && scenario == Scenario::MultilineGrid, "multiline boundary scenario parses");
    Check(TryParseScenario(L"Default", scenario) && scenario == Scenario::Default, "default boundary scenario parses");
    bool dirty = false;
    Check(TryParsePhase(L"Dirty", dirty) && dirty, "dirty boundary phase parses");
    Check(! TryParsePhase(L"dirty", dirty) && dirty, "invalid boundary phase preserves value");
    Check(TryParsePhase(L"Clean", dirty) && ! dirty, "clean boundary phase parses");
    ComplexUiModel model;
    ConfigureModel(model, Scenario::MultilineGridDistinct);
    Check(model.multilineGrid && model.cellTexts.size() == model.names.size() * 4u && model.names[0] == L"Item 0" && model.cellTexts[0] != model.cellTexts[1],
          "distinct boundary model retains short tree names and distinct column texts");
    auto parsed = WarpRasterMode::CallingThread;
    Check(TryParseRasterMode(L"Default", parsed) && parsed == WarpRasterMode::Default, "default WARP diagnostic mode parses");
    Check(! TryParseRasterMode(L"SingleThreaded", parsed) && parsed == WarpRasterMode::Default, "invalid WARP mode leaves the caller value unchanged");
    Check(TryParseRasterMode(L"CallingThread", parsed) && parsed == WarpRasterMode::CallingThread, "calling-thread WARP mode parses");
    GraphicsFixture invalid;
    Check(CreateDiagnosticTarget(invalid, static_cast<WarpRasterMode>(99)) == E_INVALIDARG && ! invalid.device, "invalid WARP policy creates no device");
    std::vector<uint8_t> expected;
    for (const auto mode : {WarpRasterMode::Default, WarpRasterMode::CallingThread})
    {
        GraphicsFixture graphics;
        graphics.width  = 64u;
        graphics.height = 32u;
        Hr(CreateDiagnosticTarget(graphics, mode), "diagnostic WARP device policy");
        Check(graphics.device->GetCreationFlags() == RequestedDeviceFlags(mode), "WARP device reports its requested creation flags");
        graphics.Bind();
        std::vector<uint8_t> pixels;
        Hr(graphics.Read(pixels), "diagnostic WARP policy completion/readback");
        Check(pixels.size() == 64u * 32u * 4u && pixels[0] == 22u && pixels[1] == 17u && pixels[2] == 14u && pixels[3] == 255u,
              "both WARP policies execute and complete the expected BGRA clear");
        if (expected.empty())
            expected = pixels;
        else
            Check(pixels == expected, "calling-thread and default WARP clear pixels match exactly");
    }
}
struct Frame
{
    std::array<int64_t, 9> qpc{};
    size_t cppAllocations     = 0u;
    size_t composeAllocations = 0u;
    int64_t updateEndQpc      = 0;
    // v3 only: CPU queries surround the frame, with their own QPC brackets.
    // The cumulative-counter interval includes probe overhead; it is not isolated frame CPU.
    std::array<int64_t, 4> cpuProbeQpc{};
    ComplexUiProfiling::CpuSample ownerCpuBefore, ownerCpuAfter;
};
struct Window
{
    bool clear = false, composite = false;
    int64_t started = 0, ended = 0;
    ComplexUiProfiling::CpuCounters before, after;
    std::array<Frame, kFrames> frames{};
};
inline int64_t Counter() noexcept
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

__declspec(noinline) inline void Run(const wchar_t* outputPath,
                                     bool timers,
                                     std::optional<WarpRasterMode> rasterMode = std::nullopt,
                                     std::optional<Scenario> selectedScenario = std::nullopt,
                                     bool dirty                               = false)
{
    const auto windows = std::make_unique<std::array<Window, 8>>();
    LARGE_INTEGER frequency{};
    Check(QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0, "boundary diagnostic QPC frequency");
    FILETIME creation{}, exit{}, kernel{}, user{};
    Check(GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user), "boundary diagnostic main-thread creation identity");
    const auto creation100ns = ComplexUiProfiling::FileTimeValue(creation);
    GraphicsFixture gpu;
    gpu.width       = 1280u;
    gpu.height      = 720u;
    const auto mode = rasterMode.value_or(WarpRasterMode::Default);
    Hr(CreateDiagnosticTarget(gpu, mode), "boundary diagnostic WARP device");
    Check(gpu.device->GetCreationFlags() == RequestedDeviceFlags(mode), "boundary diagnostic actual device flags");
    ComplexUiScene scene;
    const auto scenario = selectedScenario.value_or(Scenario::Default);
    ConfigureModel(scene.model, scenario);
    Hr(scene.Initialize(gpu.device.get()), "boundary diagnostic synthetic scene");
    if (scenario != Scenario::Default)
    {
        scene.grid->SetRowHeightDip(64.0f);
        scene.grid->SetLineClamp(2u);
    }
    D3D11_TEXTURE2D_DESC descriptor{};
    descriptor.Width = descriptor.Height = descriptor.MipLevels = descriptor.ArraySize = descriptor.SampleDesc.Count = 1u;
    descriptor.Format                                                                                                = DXGI_FORMAT_B8G8R8A8_UNORM;
    descriptor.Usage                                                                                                 = D3D11_USAGE_STAGING;
    descriptor.CPUAccessFlags                                                                                        = D3D11_CPU_ACCESS_READ;
    wil::com_ptr_nothrow<ID3D11Texture2D> completion;
    Hr(gpu.device->CreateTexture2D(&descriptor, nullptr, completion.put()), "boundary diagnostic reusable completion pixel");
    const D3D11_BOX pixel{0u, 0u, 0u, 1u, 1u, 1u};
    const auto readback = [&]
    {
        gpu.context->CopySubresourceRegion(completion.get(), 0u, 0u, 0u, 0u, gpu.target.get(), 0u, &pixel);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Hr(gpu.context->Map(completion.get(), 0u, D3D11_MAP_READ, 0u, &mapped), "boundary diagnostic warm completion");
        gpu.context->Unmap(completion.get(), 0u);
    };
    for (size_t frame = 0u; frame < 200u; ++frame)
    {
        scene.Update(frame);
        Hr(scene.view.Prepare(1280u, 720u), "boundary diagnostic fixed dirty warmup");
        gpu.Bind();
        Hr(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "boundary diagnostic warm composition");
        readback();
    }
    Hr(gpu.Save(std::filesystem::path(outputPath).concat(L".png")), "boundary diagnostic populated capture before probes");
    bool complete = true;
    for (size_t index = 0u; index < windows->size(); ++index)
    {
        auto& window = (*windows)[index];
        // Forward then reverse controls expose order effects; these workloads are not
        // algebraically subtractable estimates of individual GPU operations.
        const size_t kind = index < 4u ? index : 7u - index;
        window.clear      = kind == 0u || kind == 1u;
        window.composite  = kind == 0u || kind == 2u;
        window.before     = ComplexUiProfiling::ReadCpu();
        const auto before = scene.view.GetStatistics();
        window.started    = Counter();
        size_t frameIndex = 0u;
        for (auto& frame : window.frames)
        {
            if (selectedScenario)
            {
                frame.cpuProbeQpc[0] = Counter();
                frame.ownerCpuBefore = ComplexUiProfiling::SampleCpu(GetCurrentThread(), false);
                frame.cpuProbeQpc[1] = Counter();
            }
            allocations      = 0u;
            countAllocations = true;
            frame.qpc[0]     = Counter();
            if (dirty)
                scene.Update(200u + frameIndex);
            if (selectedScenario && timers)
                frame.updateEndQpc = Counter();
            const HRESULT prepared = scene.view.Prepare(1280u, 720u);
            if (timers)
                frame.qpc[1] = Counter();
            auto* target = gpu.rtv.get();
            gpu.context->OMSetRenderTargets(1u, &target, nullptr);
            if (timers)
                frame.qpc[2] = Counter();
            if (window.clear)
            {
                constexpr float background[]{0.055f, 0.065f, 0.085f, 1.0f};
                gpu.context->ClearRenderTargetView(target, background);
            }
            if (timers)
                frame.qpc[3] = Counter();
            const auto allocationStart = allocations;
            const HRESULT composited   = window.composite ? scene.view.Composite(gpu.context.get(), gpu.Viewport()) : S_FALSE;
            frame.composeAllocations   = allocations - allocationStart;
            if (timers)
                frame.qpc[4] = Counter();
            gpu.context->CopySubresourceRegion(completion.get(), 0u, 0u, 0u, 0u, gpu.target.get(), 0u, &pixel);
            if (timers)
                frame.qpc[5] = Counter();
            D3D11_MAPPED_SUBRESOURCE mapped{};
            const HRESULT mappedResult = gpu.context->Map(completion.get(), 0u, D3D11_MAP_READ, 0u, &mapped);
            if (timers)
                frame.qpc[6] = Counter();
            if (SUCCEEDED(mappedResult))
                gpu.context->Unmap(completion.get(), 0u);
            if (timers)
                frame.qpc[7] = Counter();
            frame.qpc[8]     = Counter();
            countAllocations = false;
            if (selectedScenario)
            {
                frame.cpuProbeQpc[2] = Counter();
                frame.ownerCpuAfter  = ComplexUiProfiling::SampleCpu(GetCurrentThread(), false);
                frame.cpuProbeQpc[3] = Counter();
                complete = complete && ComplexUiProfiling::CpuComplete(frame.ownerCpuBefore) && ComplexUiProfiling::CpuComplete(frame.ownerCpuAfter);
            }
            frame.cppAllocations = allocations;
            Check(prepared == (dirty ? S_OK : S_FALSE), "boundary preparation matches the explicit phase");
            Check(composited == (window.composite ? S_OK : S_FALSE), "boundary controlled composition");
            Hr(mappedResult, "boundary blocking one-pixel map");
            Check(dirty ? frame.cppAllocations <= ComplexUiBenchmark::kDirtyAllocationsPerFrameCeiling : frame.cppAllocations == 0u,
                  "boundary whole-frame allocations respect the explicit phase budget");
            Check(frame.composeAllocations == 0u, "boundary composition allocates no C++ heap storage");
            ++frameIndex;
        }
        window.ended     = Counter();
        window.after     = ComplexUiProfiling::ReadCpu();
        const auto after = scene.view.GetStatistics();
        Check(after.preparations - before.preparations == (dirty ? kFrames : 0u) && after.surfaceAllocations == before.surfaceAllocations &&
                  after.surfaceBytes == before.surfaceBytes && after.composites - before.composites == (window.composite ? kFrames : 0u),
              "boundary controls preserve preparation and surface invariants");
        complete = complete && ComplexUiProfiling::CpuComplete(window.before.main) && ComplexUiProfiling::CpuComplete(window.before.process) &&
                   ComplexUiProfiling::CpuComplete(window.after.main) && ComplexUiProfiling::CpuComplete(window.after.process);
    }
    scene.view.SetVisible(false);
    const auto hidden = scene.view.GetStatistics();
    Check(scene.view.Prepare(1280u, 720u) == S_FALSE && scene.view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE && hidden.surfaceBytes == 0u &&
              scene.view.GetStatistics().preparations == hidden.preparations && scene.view.GetStatistics().composites == hidden.composites,
          "boundary hidden view releases surface and does no work");
    std::ofstream output{std::filesystem::path(outputPath)};
    Check(bool(output), "boundary output file");
    const auto schema = selectedScenario ? 3 : rasterMode ? 2 : 1;
    output << std::setprecision(17) << "{\"schema\":" << schema << ",\"fixture\":\"dxui-complex-ui-boundaries-v" << schema
           << "\",\"classification\":\"diagnostic-only\",\"renderer\":\"WARP\","
              "\"scenario\":\""
           << ScenarioName(scenario)
           << "\",\"heapSampling\":false,\"width\":1280,\"height\":720,\"dpi\":96,\"controls\":83,\"modelRows\":1000,"
              "\"warmFrames\":200,\"probeFrames\":320,\"hiddenPreparations\":0,\"hiddenComposites\":0,\"hiddenSurfaceBytes\":0,"
           << "\"compiler\":" << _MSC_FULL_VER << ",\"iteratorDebugLevel\":" << _ITERATOR_DEBUG_LEVEL << ",\"pointerBits\":" << sizeof(void*) * 8u
           << ",\"processId\":" << GetCurrentProcessId() << ",\"mainThreadId\":" << GetCurrentThreadId() << ",\"mainThreadCreation100ns\":" << creation100ns
           << ",\"boundaryTimers\":" << (timers ? "true" : "false") << ",\"telemetryCompleteness\":\"" << (complete ? "complete" : "partial")
           << "\",\"fixedDiagnosticStorageBytes\":" << sizeof(*windows)
           << ",\"clock\":{\"implementation\":\"QueryPerformanceCounter\",\"ticksPerSecond\":" << frequency.QuadPart
           << ",\"tickNanoseconds\":" << 1.0e9 / static_cast<double>(frequency.QuadPart)
           << "},"
              "\"timingScope\":\"Absolute QPC edges: frame start, prepare end, bind end, clear end, composite end, copy end, blocking Map end, Unmap end, "
              "frame end. "
              "Interior edges are zero when timers are off. Controls omit clear and/or composition; preparation, binding and one-pixel completion remain.\","
              "\"schedulingScope\":\"Per-window main/process cumulative counters are sampled outside frames. QPC edges permit external ETW alignment; no "
              "scheduler-delay, GPU-duration or dedicated-host attribution.\",";
    if (rasterMode)
        output << "\"warpRasterMode\":\"" << (mode == WarpRasterMode::CallingThread ? "CallingThread" : "Default")
               << "\",\"requestedDeviceFlags\":" << RequestedDeviceFlags(mode) << ",\"deviceCreationFlags\":" << gpu.device->GetCreationFlags() << ',';
    if (selectedScenario)
        output
            << "\"phase\":\"" << (dirty ? "Dirty" : "Clean") << "\",\"firstUpdate\":" << (dirty ? 200u : 0u)
            << ",\"dirtyAllocationCeilingPerFrame\":" << ComplexUiBenchmark::kDirtyAllocationsPerFrameCeiling
            << ",\"ownerCpuScope\":\"Cumulative main-thread kernel/user 100ns and cycle readings surround each frame. CPU intervals include the query probes; "
               "zero coarse time deltas are valid. CPU probe QPC brackets expose observer cost, not scheduler delay or GPU time.\",";
    output << "\"windows\":[";
    for (size_t index = 0u; index < windows->size(); ++index)
    {
        if (index)
            output << ',';
        const auto& window = (*windows)[index];
        output << "{\"clear\":" << (window.clear ? "true" : "false") << ",\"composite\":" << (window.composite ? "true" : "false")
               << ",\"startedQpc\":" << window.started << ",\"endedQpc\":" << window.ended << ",\"cpuBefore\":";
        ComplexUiProfiling::WriteCounters(output, window.before);
        output << ",\"cpuAfter\":";
        ComplexUiProfiling::WriteCounters(output, window.after);
        output << ",\"frames\":[";
        for (size_t indexFrame = 0u; indexFrame < window.frames.size(); ++indexFrame)
        {
            if (indexFrame)
                output << ',';
            const auto& frame = window.frames[indexFrame];
            output << "{\"cppAllocations\":" << frame.cppAllocations << ",\"qpc\":[";
            for (size_t edge = 0u; edge < frame.qpc.size(); ++edge)
            {
                if (edge)
                    output << ',';
                output << frame.qpc[edge];
            }
            output << ']';
            if (selectedScenario)
            {
                output << ",\"composeAllocations\":" << frame.composeAllocations << ",\"updateEndQpc\":" << frame.updateEndQpc << ",\"cpuProbeQpc\":[";
                for (size_t edge = 0u; edge < frame.cpuProbeQpc.size(); ++edge)
                {
                    if (edge)
                        output << ',';
                    output << frame.cpuProbeQpc[edge];
                }
                output << "],\"ownerCpuBefore\":";
                ComplexUiProfiling::WriteCpu(output, frame.ownerCpuBefore);
                output << ",\"ownerCpuAfter\":";
                ComplexUiProfiling::WriteCpu(output, frame.ownerCpuAfter);
            }
            output << '}';
        }
        output << "]}";
    }
    output << "]}\n";
    output.close();
    Check(bool(output), "boundary complete report written");
}
} // namespace ComplexUiBoundaries
