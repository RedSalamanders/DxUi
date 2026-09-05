#pragma once
#include "../../Samples/ComplexUi/ComplexUiScene.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

// Fixture-only work: one reusable staging pixel blocks for completed GPU work. Never used in library rendering.
namespace ComplexUiBenchmark
{
inline PROCESS_MEMORY_COUNTERS_EX Memory()
{
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    Check(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE, "benchmark process memory");
    return memory;
}

inline void Run(const wchar_t* outputPath)
{
    GraphicsFixture gpu;
    gpu.width  = 1280;
    gpu.height = 720;
    Hr(gpu.Create(), "benchmark WARP device");
    ComplexUiScene scene;
    Hr(scene.Initialize(gpu.device.get()), "benchmark independent scene");
    auto& view = scene.view;

    D3D11_TEXTURE2D_DESC readDesc{};
    readDesc.Width = readDesc.Height = readDesc.MipLevels = readDesc.ArraySize = readDesc.SampleDesc.Count = 1;
    readDesc.Format                                                                                        = DXGI_FORMAT_B8G8R8A8_UNORM;
    readDesc.Usage                                                                                         = D3D11_USAGE_STAGING;
    readDesc.CPUAccessFlags                                                                                = D3D11_CPU_ACCESS_READ;
    wil::com_ptr_nothrow<ID3D11Texture2D> completion;
    Hr(gpu.device->CreateTexture2D(&readDesc, nullptr, completion.put()), "benchmark completion texture");
    const auto complete = [&]
    {
        const D3D11_BOX pixel{0, 0, 0, 1, 1, 1};
        gpu.context->CopySubresourceRegion(completion.get(), 0, 0, 0, 0, gpu.target.get(), 0, &pixel);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Hr(gpu.context->Map(completion.get(), 0, D3D11_MAP_READ, 0, &mapped), "benchmark GPU completion");
        gpu.context->Unmap(completion.get(), 0);
    };
    const auto update = [&](size_t frame) { scene.Update(frame); };
    for (size_t frame = 0; frame < 20; ++frame)
    {
        update(frame);
        Hr(view.Prepare(1280, 720), "benchmark warm preparation");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "benchmark warm composition");
        complete();
    }
    // Capture once outside measurement; reviewable proof that the workload has populated controls.
    Hr(gpu.Save(L".build/test-artifacts/complex-ui.png"), "complex UI screenshot");
    std::ofstream output{std::filesystem::path(outputPath)};
    Check(bool(output), "benchmark output file");
    output << std::setprecision(10) << "{\"compiler\":" << _MSC_FULL_VER
           << ",\"fixture\":\"dxui-complex-ui-v2\",\"renderer\":\"WARP\",\"width\":1280,\"height\":720,\"dpi\":96,"
           << "\"controls\":83,\"modelRows\":1000,\"framesPerRound\":40,\"roundCount\":5,\"scenarios\":[";
    using Clock        = std::chrono::steady_clock;
    const auto elapsed = [](Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); };
    for (int dirty = 0; dirty != 2; ++dirty)
    {
        if (dirty)
            output << ',';
        output << "{\"name\":\"" << (dirty ? "dirty" : "clean") << "\",\"rounds\":[";
        for (size_t round = 0; round < 5; ++round)
        {
            const auto before       = view.GetStatistics();
            const auto memoryBefore = Memory();
            auto memoryPeak         = memoryBefore;
            std::array<double, 40> frameMs{}, prepareMs{}, composeMs{};
            size_t cppAllocations     = 0;
            size_t composeAllocations = 0;
            for (size_t frame = 0; frame < frameMs.size(); ++frame)
            {
                allocations      = 0;
                countAllocations = true;
                const auto start = Clock::now();
                if (dirty)
                    update(20 + round * frameMs.size() + frame);
                const auto preparationStart = Clock::now();
                Hr(view.Prepare(1280, 720), "benchmark preparation");
                prepareMs[frame] = elapsed(preparationStart);
                gpu.Bind();
                const auto allocationStart  = allocations;
                const auto compositionStart = Clock::now();
                Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "benchmark composition");
                composeMs[frame] = elapsed(compositionStart);
                composeAllocations += allocations - allocationStart;
                complete();
                frameMs[frame]   = elapsed(start);
                countAllocations = false;
                cppAllocations += allocations;
                const auto memory         = Memory();
                memoryPeak.PrivateUsage   = (std::max)(memoryPeak.PrivateUsage, memory.PrivateUsage);
                memoryPeak.WorkingSetSize = (std::max)(memoryPeak.WorkingSetSize, memory.WorkingSetSize);
            }
            const auto after       = view.GetStatistics();
            const auto memoryAfter = Memory();
            double totalMs         = 0;
            for (double value : frameMs)
                totalMs += value;
            std::sort(frameMs.begin(), frameMs.end());
            std::sort(prepareMs.begin(), prepareMs.end());
            std::sort(composeMs.begin(), composeMs.end());
            Check(composeAllocations == 0, "complex composition has no C++ allocations");
            Check(after.surfaceAllocations == before.surfaceAllocations, "complex updates reuse surface");
            Check(after.preparations - before.preparations == (dirty ? frameMs.size() : 0), "complex preparation count");
            if (round)
                output << ',';
            output << "{\"fps\":" << 40000 / totalMs << ",\"frameP50Ms\":" << frameMs[19] << ",\"frameP95Ms\":" << frameMs[37]
                   << ",\"prepareP95Ms\":" << prepareMs[37] << ",\"composeCpuP95Ms\":" << composeMs[37] << ",\"cppAllocations\":" << cppAllocations
                   << ",\"composeAllocations\":" << composeAllocations << ",\"surfaceBytes\":" << after.surfaceBytes
                   << ",\"replacementPeakBytes\":" << after.replacementPeakBytes << ",\"privateBytes\":" << memoryAfter.PrivateUsage
                   << ",\"privatePeakBytes\":" << memoryPeak.PrivateUsage
                   << ",\"privateGrowthBytes\":" << static_cast<int64_t>(memoryAfter.PrivateUsage) - static_cast<int64_t>(memoryBefore.PrivateUsage)
                   << ",\"workingSetBytes\":" << memoryAfter.WorkingSetSize << ",\"workingSetPeakBytes\":" << memoryPeak.WorkingSetSize << '}';
            std::cout << "Complex UI " << (dirty ? "dirty" : "clean") << " round " << round + 1 << ": " << 40000 / totalMs << " completed offscreen FPS; p95 "
                      << frameMs[37] << " ms; private " << memoryAfter.PrivateUsage << " bytes\n";
        }
        output << "]}";
    }
    view.SetVisible(false);
    const auto hidden = view.GetStatistics();
    Check(! view.NeedsAnimation() && ! view.NeedsPreparation(), "complex hidden view requests no work");
    Check(view.Prepare(1280, 720) == S_FALSE, "hidden benchmark preparation skipped");
    Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "hidden benchmark composition skipped");
    Check(view.GetStatistics().preparations == hidden.preparations && view.GetStatistics().composites == hidden.composites, "hidden counters unchanged");
    output << "],\"hiddenPreparations\":0,\"hiddenComposites\":0}\n";
    output.close();
    Check(bool(output), "benchmark report written");
}
} // namespace ComplexUiBenchmark
