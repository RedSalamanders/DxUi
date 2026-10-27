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
    size_t cppAllocations = 0u;
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

__declspec(noinline) inline void Run(const wchar_t* outputPath, bool timers, std::optional<WarpRasterMode> rasterMode = std::nullopt)
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
    Hr(scene.Initialize(gpu.device.get()), "boundary diagnostic synthetic scene");
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
        for (auto& frame : window.frames)
        {
            allocations            = 0u;
            countAllocations       = true;
            frame.qpc[0]           = Counter();
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
            const HRESULT composited = window.composite ? scene.view.Composite(gpu.context.get(), gpu.Viewport()) : S_FALSE;
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
            frame.qpc[8]         = Counter();
            countAllocations     = false;
            frame.cppAllocations = allocations;
            Check(prepared == S_FALSE, "boundary clean preparation is a no-op");
            Check(composited == (window.composite ? S_OK : S_FALSE), "boundary controlled composition");
            Hr(mappedResult, "boundary blocking one-pixel map");
            Check(frame.cppAllocations == 0u, "boundary probes allocate no C++ heap storage");
        }
        window.ended     = Counter();
        window.after     = ComplexUiProfiling::ReadCpu();
        const auto after = scene.view.GetStatistics();
        Check(after.preparations == before.preparations && after.surfaceAllocations == before.surfaceAllocations && after.surfaceBytes == before.surfaceBytes &&
                  after.composites - before.composites == (window.composite ? kFrames : 0u),
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
    const auto schema = rasterMode ? 2 : 1;
    output << std::setprecision(17) << "{\"schema\":" << schema << ",\"fixture\":\"dxui-complex-ui-boundaries-v" << schema
           << "\",\"classification\":\"diagnostic-only\",\"renderer\":\"WARP\","
              "\"scenario\":\"Default\",\"heapSampling\":false,\"width\":1280,\"height\":720,\"dpi\":96,\"controls\":83,\"modelRows\":1000,"
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
            output << "]}";
        }
        output << "]}";
    }
    output << "]}\n";
    output.close();
    Check(bool(output), "boundary complete report written");
}
} // namespace ComplexUiBoundaries
