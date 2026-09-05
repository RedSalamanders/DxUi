#pragma once
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

// Fixture-only work: one reusable staging pixel blocks for completed GPU work. Never used in library rendering.
namespace ComplexUiBenchmark
{
struct Model final : DxUi::IDxGridModel, DxUi::IDxTreeModel
{
    std::array<std::wstring, 1000> names;
    Model()
    {
        for (size_t i = 0; i < names.size(); ++i)
            names[i] = L"Dashboard item " + std::to_wstring(i);
    }
    size_t GetRowCount() const noexcept override
    {
        return names.size();
    }
    size_t GetColumnCount() const noexcept override
    {
        return 4;
    }
    DxUi::GridColumnDesc GetColumn(size_t column) const override
    {
        return {std::to_wstring(column), L"Column " + std::to_wstring(column), 170};
    }
    void GetCellData(size_t row, size_t, DxUi::GridCellData& cell) const override
    {
        cell.text = names[row];
    }
    std::optional<size_t> FindRowByStableId(uint64_t id) const noexcept override
    {
        return id < names.size() ? std::optional<size_t>(static_cast<size_t>(id)) : std::nullopt;
    }
    uint64_t GetStableRowId(size_t row) const noexcept override
    {
        return row;
    }
    size_t GetVisibleItemCount() const noexcept override
    {
        return names.size();
    }
    void GetVisibleItem(size_t row, DxUi::TreeItemData& item) const override
    {
        item.id    = row;
        item.text  = names[row];
        item.depth = static_cast<uint32_t>(row % 3);
    }
};

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
    Model model;
    DxUi::EmbeddedHost view;
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "benchmark shared pool");
    Hr(view.Attach(pool), "benchmark view");
    auto theme          = DxUi::MakeDefaultThemePalette(true);
    theme.reducedMotion = true;
    view.Controls().SetTheme(theme);
    auto root = std::make_unique<DxUi::Panel>();
    std::array<DxUi::Slider*, 16> sliders{};
    std::array<DxUi::ProgressBar*, 16> progress{};
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        auto* card    = root->AddChild<DxUi::CardPanel>();
        const float x = 8 + static_cast<float>(i % 4) * 316;
        const float y = 8 + static_cast<float>(i / 4) * 86;
        card->SetBounds(D2D1::RectF(x, y, x + 304, y + 78));
        card->AddChild<DxUi::Label>(L"Channel " + std::to_wstring(i))->SetBounds(D2D1::RectF(x + 8, y + 2, x + 155, y + 26));
        auto* toggle = card->AddChild<DxUi::Toggle>(L"Enabled");
        toggle->SetBounds(D2D1::RectF(x + 166, y + 2, x + 296, y + 30));
        toggle->SetChecked(i % 2 == 0);
        sliders[i] = card->AddChild<DxUi::Slider>();
        sliders[i]->SetBounds(D2D1::RectF(x + 8, y + 30, x + 296, y + 58));
        progress[i] = card->AddChild<DxUi::ProgressBar>();
        progress[i]->SetBounds(D2D1::RectF(x + 8, y + 62, x + 296, y + 74));
    }
    auto* tree = root->AddChild<DxUi::Tree>();
    tree->SetBounds(D2D1::RectF(8, 360, 380, 712));
    tree->SetModel(&model);
    auto* grid = root->AddChild<DxUi::Grid>();
    grid->SetBounds(D2D1::RectF(392, 360, 1272, 712));
    grid->SetModel(&model);
    view.Controls().SetRoot(std::move(root));

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
    const auto update = [&](size_t frame)
    {
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            const double value = static_cast<double>((frame + i) % 100);
            sliders[i]->SetValue(value);
            progress[i]->SetValue(value);
        }
        grid->EnsureRowVisible((frame * 7) % model.names.size());
        view.MarkDirty();
    };
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
           << ",\"fixture\":\"complex-ui-v1\",\"renderer\":\"WARP\",\"width\":1280,\"height\":720,\"dpi\":96,"
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
