#pragma once

#include "../../Samples/ComplexUi/ComplexUiScene.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>

// Opt-in diagnostic only. The ordinary benchmark's timed loop is untouched.
namespace ComplexUiFrameStages
{
inline void Run(const wchar_t* outputPath)
{
    GraphicsFixture gpu;
    gpu.width  = 1280;
    gpu.height = 720;
    Hr(gpu.Create(), "stage diagnostic WARP device");
    ComplexUiScene scene;
    Hr(scene.Initialize(gpu.device.get()), "stage diagnostic independent scene");
    auto& view = scene.view;

    D3D11_TEXTURE2D_DESC readDesc{};
    readDesc.Width = readDesc.Height = readDesc.MipLevels = readDesc.ArraySize = readDesc.SampleDesc.Count = 1;
    readDesc.Format                                                                                        = DXGI_FORMAT_B8G8R8A8_UNORM;
    readDesc.Usage                                                                                         = D3D11_USAGE_STAGING;
    readDesc.CPUAccessFlags                                                                                = D3D11_CPU_ACCESS_READ;
    wil::com_ptr_nothrow<ID3D11Texture2D> completion;
    Hr(gpu.device->CreateTexture2D(&readDesc, nullptr, completion.put()), "stage diagnostic completion texture");
    const auto complete = [&]
    {
        const D3D11_BOX pixel{0, 0, 0, 1, 1, 1};
        gpu.context->CopySubresourceRegion(completion.get(), 0, 0, 0, 0, gpu.target.get(), 0, &pixel);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Hr(gpu.context->Map(completion.get(), 0, D3D11_MAP_READ, 0, &mapped), "stage diagnostic GPU completion");
        gpu.context->Unmap(completion.get(), 0);
    };
    const auto update = [&](size_t frame) { scene.Update(frame); };
    for (size_t frame = 0; frame < 20; ++frame)
    {
        update(frame);
        Hr(view.Prepare(1280, 720), "stage diagnostic warm preparation");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "stage diagnostic warm composition");
        complete();
    }
    Hr(gpu.Save(L".build/test-artifacts/complex-ui.png"), "stage diagnostic screenshot");

    using Clock = std::chrono::steady_clock;
    constexpr std::array names{"update", "prepare", "bind", "composeCpu", "gpuCompletion"};
    std::ofstream output{std::filesystem::path(outputPath)};
    Check(bool(output), "stage diagnostic output file");
    output << std::setprecision(10) << R"({"fixture":"dxui-complex-ui-frame-stages-v1","renderer":"WARP",)"
           << R"("width":1280,"height":720,"dpi":96,"controls":83,"modelRows":1000,)"
           << R"("warmFrames":20,"framesPerRound":40,"roundCount":5,"scenarios":[)";
    for (int dirty = 0; dirty != 2; ++dirty)
    {
        if (dirty)
            output << ',';
        output << R"({"name":")" << (dirty ? "dirty" : "clean") << R"(","rounds":[)";
        for (size_t round = 0; round < 5; ++round)
        {
            const auto before = view.GetStatistics();
            std::array<std::array<Clock::time_point, 6>, 40> times{};
            for (size_t frame = 0; frame < times.size(); ++frame)
            {
                auto& t = times[frame];
                t[0]    = Clock::now();
                if (dirty)
                    update(20 + round * times.size() + frame);
                t[1] = Clock::now();
                Hr(view.Prepare(1280, 720), "stage diagnostic preparation");
                t[2] = Clock::now();
                gpu.Bind();
                t[3] = Clock::now();
                Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "stage diagnostic composition");
                t[4] = Clock::now();
                complete();
                t[5] = Clock::now();
            }
            const auto after = view.GetStatistics();
            Check(after.surfaceAllocations == before.surfaceAllocations, "stage diagnostic reuses surface");
            Check(after.preparations - before.preparations == (dirty ? times.size() : 0u), "stage diagnostic preparation count");
            std::array<double, 5> stageTotals{};
            std::array<std::array<double, 40>, 5> stageSamples{};
            std::array<double, 40> frameSamples{};
            double frameTotal = 0.0;
            for (size_t frame = 0; frame < times.size(); ++frame)
            {
                for (size_t stage = 0; stage < stageTotals.size(); ++stage)
                {
                    const double ms = std::chrono::duration<double, std::milli>(times[frame][stage + 1] - times[frame][stage]).count();
                    stageTotals[stage] += ms;
                    stageSamples[stage][frame] = ms;
                }
                frameSamples[frame] = std::chrono::duration<double, std::milli>(times[frame][5] - times[frame][0]).count();
                frameTotal += frameSamples[frame];
            }
            for (auto& samples : stageSamples)
                std::sort(samples.begin(), samples.end());
            std::sort(frameSamples.begin(), frameSamples.end());
            if (round)
                output << ',';
            output << R"({"round":)" << round << R"(,"frameTotalMs":)" << frameTotal << R"(,"frameP95Ms":)" << frameSamples[37] << R"(,"surfaceBytes":)"
                   << after.surfaceBytes << R"(,"stages":{)";
            for (size_t stage = 0; stage < stageTotals.size(); ++stage)
            {
                if (stage)
                    output << ',';
                output << '"' << names[stage] << R"(":{"totalMs":)" << stageTotals[stage] << R"(,"p95Ms":)" << stageSamples[stage][37] << '}';
            }
            output << "}}";
        }
        output << "]}";
    }
    view.SetVisible(false);
    const auto hidden = view.GetStatistics();
    Check(! view.NeedsAnimation() && ! view.NeedsPreparation(), "stage diagnostic hidden view requests no work");
    Check(view.Prepare(1280, 720) == S_FALSE, "stage diagnostic hidden preparation skipped");
    Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "stage diagnostic hidden composition skipped");
    Check(view.GetStatistics().preparations == hidden.preparations && view.GetStatistics().composites == hidden.composites,
          "stage diagnostic hidden counters unchanged");
    output << R"(],"hiddenPreparations":0,"hiddenComposites":0})" << '\n';
    output.close();
    Check(bool(output), "stage diagnostic report written");
}
} // namespace ComplexUiFrameStages
