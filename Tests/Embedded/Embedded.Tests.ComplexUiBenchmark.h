#pragma once
#include "../../Samples/ComplexUi/ComplexUiScene.h"
#include "../Support/Support.Tests.HeapDiagnostic.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <psapi.h>
#include <thread>
#pragma comment(lib, "psapi.lib")

// Fixture-only work: one reusable staging pixel blocks for completed GPU work. Never used in library rendering.
namespace ComplexUiBenchmark
{
// Whole-frame C++ allocation ceilings (Specs/Core/Core_PerformanceAndResources.md). Clean rounds allocate nothing.
// Debug STL (_ITERATOR_DEBUG_LEVEL 2) allocates one container proxy per std::vector/std::wstring, so the Debug
// dirty ceiling is separate from the Release ceiling of 64 allocations per dirty frame.
#if _ITERATOR_DEBUG_LEVEL != 0
inline constexpr size_t kDirtyAllocationsPerFrameCeiling = 320;
#else
inline constexpr size_t kDirtyAllocationsPerFrameCeiling = 64;
#endif

inline PROCESS_MEMORY_COUNTERS_EX Memory()
{
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    Check(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != FALSE, "benchmark process memory");
    return memory;
}

// Opt-in diagnostic outside timed rounds. The shared helper walks only this
// process's heaps, one lock at a time with no output or allocations while
// walking, and reports per-heap errors so partial accounting is never total use.
inline void WriteHeapDiagnostic(std::ostream& output)
{
    DxUiTestSupport::WriteHeapDiagnostic(output, [](bool ok, const char* reason) { Check(ok, reason); });
}

// Distinct per-column text: every visible cell differs, so a layout cache keyed by value must retain one layout per
// visible cell. The repeated-name multiline fixture shows one text across a row and hides cache conflicts.
inline void FillDistinctCellTexts(ComplexUiModel& model)
{
    static constexpr std::array<std::wstring_view, 4> openings{L"Élément ", L"Synchronisation ", L"Chemin C:\\Données\\Projet ", L"État de la revue "};
    static constexpr std::array<std::wstring_view, 4> endings{
        L" : vérifier la configuration du serveur principal.\nDeuxième ligne avec é et 📷.",
        L" : échec après trois tentatives, consulter le journal détaillé.\nNouvelle tentative prévue.",
        L"\\rapport final révisé.docx\nModifié par l’équipe de validation.",
        L" : en attente de validation par la responsable du service.\nPriorité élevée.",
    };
    model.cellTexts.clear();
    model.cellTexts.reserve(model.names.size() * openings.size());
    for (size_t row = 0u; row < model.names.size(); ++row)
        for (size_t column = 0u; column < openings.size(); ++column)
            model.cellTexts.push_back(std::wstring(openings[column]) + std::to_wstring(row) + std::wstring(endings[column]));
}

inline void Run(
    const wchar_t* outputPath, bool multilineGrid = false, bool retention = false, bool heapDiagnostic = false, bool paced = false, bool distinctCells = false)
{
    // Stage samples stay outside frame timing and help distinguish initialization,
    // image encoding and retained rendering costs when process totals regress.
    std::array<PROCESS_MEMORY_COUNTERS_EX, 6> memoryPhases{};
    memoryPhases[0] = Memory();
    GraphicsFixture gpu;
    gpu.width  = 1280;
    gpu.height = 720;
    Hr(gpu.Create(), "benchmark WARP device");
    memoryPhases[1] = Memory();
    ComplexUiScene scene;
    scene.model.multilineGrid = multilineGrid;
    // The distinct scene keeps the Tree's short names: the long names below reach the Tree too, whose single-line
    // rows then draw the emoji through the color-font path every frame and dominate the frame instead of the grid.
    if (multilineGrid && ! distinctCells)
        for (size_t i = 0u; i < scene.model.names.size(); ++i)
            scene.model.names[i] = L"Description française détaillée de l’élément " + std::to_wstring(i) +
                                   L" : vérifier les informations avant de poursuivre.\nUne deuxième phrase complète avec é et 📷.";
    if (distinctCells)
        FillDistinctCellTexts(scene.model);
    Hr(scene.Initialize(gpu.device.get()), "benchmark independent scene");
    if (multilineGrid)
    {
        scene.grid->SetRowHeightDip(64.0f);
        scene.grid->SetLineClamp(2u);
    }
    memoryPhases[2] = Memory();
    auto& view      = scene.view;

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
    memoryPhases[3] = Memory();
    // Capture once outside measurement; reviewable proof that the workload has populated controls.
    Hr(gpu.Save(distinctCells   ? L".build/test-artifacts/complex-ui-multiline-grid-distinct.png"
                : multilineGrid ? L".build/test-artifacts/complex-ui-multiline-grid.png"
                                : L".build/test-artifacts/complex-ui.png"),
       "complex UI screenshot");
    memoryPhases[4] = Memory();
    std::ofstream output{std::filesystem::path(outputPath)};
    Check(bool(output), "benchmark output file");
    using Clock = std::chrono::steady_clock;
    static_assert(Clock::period::num == 1 && Clock::period::den == 1'000'000'000, "review clock diagnostics if the MSVC clock period changes");
    LARGE_INTEGER clockFrequency{};
    Check(QueryPerformanceFrequency(&clockFrequency) != FALSE && clockFrequency.QuadPart > 0, "benchmark clock frequency");
    output << std::setprecision(10) << "{\"compiler\":" << _MSC_FULL_VER << ",\"fixture\":\""
           << (paced            ? "dxui-complex-ui-multiline-grid-heap-paced-v1"
               : heapDiagnostic ? "dxui-complex-ui-multiline-grid-heap-v1"
               : retention      ? "dxui-complex-ui-multiline-grid-retention-v1"
               : distinctCells  ? "dxui-complex-ui-multiline-grid-distinct-v1"
               : multilineGrid  ? "dxui-complex-ui-multiline-grid-v1"
                                : "dxui-complex-ui-v2")
           << "\",\"renderer\":\"WARP\",\"width\":1280,\"height\":720,\"dpi\":96,"
           << "\"controls\":83,\"modelRows\":1000,\"framesPerRound\":40,\"roundCount\":5,\"dirtyAllocationCeilingPerFrame\":"
           << kDirtyAllocationsPerFrameCeiling
           << ",\"clock\":{\"name\":\"std::chrono::steady_clock\",\"implementation\":\"QueryPerformanceCounter\",\"ticksPerSecond\":" << clockFrequency.QuadPart
           << ",\"tickNanoseconds\":" << 1.0e9 / static_cast<double>(clockFrequency.QuadPart)
           << ",\"nominalPeriodNanoseconds\":" << 1.0e9 * static_cast<double>(Clock::period::num) / static_cast<double>(Clock::period::den)
           << "},\"scenarios\":[";
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
            // Preserve frame order outside the measured/allocation-counted interval. Sorting still computes the same judged percentiles.
            const auto rawFrameMs = frameMs, rawPrepareMs = prepareMs, rawComposeMs = composeMs;
            std::sort(frameMs.begin(), frameMs.end());
            std::sort(prepareMs.begin(), prepareMs.end());
            std::sort(composeMs.begin(), composeMs.end());
            Check(composeAllocations == 0, "complex composition has no C++ allocations");
            Check(after.surfaceAllocations == before.surfaceAllocations, "complex updates reuse surface");
            Check(after.preparations - before.preparations == (dirty ? frameMs.size() : 0), "complex preparation count");
            Check(dirty ? cppAllocations <= kDirtyAllocationsPerFrameCeiling * frameMs.size() : cppAllocations == 0,
                  dirty ? "complex dirty rounds stay within the C++ allocation ceiling" : "complex clean rounds make no C++ allocations");
            if (round)
                output << ',';
            output << "{\"fps\":" << 40000 / totalMs << ",\"frameP50Ms\":" << frameMs[19] << ",\"frameP95Ms\":" << frameMs[37]
                   << ",\"prepareP95Ms\":" << prepareMs[37] << ",\"composeCpuP95Ms\":" << composeMs[37] << ",\"cppAllocations\":" << cppAllocations
                   << ",\"composeAllocations\":" << composeAllocations << ",\"surfaceBytes\":" << after.surfaceBytes
                   << ",\"replacementPeakBytes\":" << after.replacementPeakBytes << ",\"privateBytes\":" << memoryAfter.PrivateUsage
                   << ",\"privatePeakBytes\":" << memoryPeak.PrivateUsage
                   << ",\"privateGrowthBytes\":" << static_cast<int64_t>(memoryAfter.PrivateUsage) - static_cast<int64_t>(memoryBefore.PrivateUsage)
                   << ",\"workingSetBytes\":" << memoryAfter.WorkingSetSize << ",\"workingSetPeakBytes\":" << memoryPeak.WorkingSetSize;
            const auto writeSamples = [&](const auto& samples)
            {
                output << '[';
                for (size_t index = 0; index < samples.size(); ++index)
                {
                    if (index)
                        output << ',';
                    output << samples[index];
                }
                output << ']';
            };
            output << ",\"timingSamplesMs\":{\"frame\":";
            writeSamples(rawFrameMs);
            output << ",\"prepare\":";
            writeSamples(rawPrepareMs);
            output << ",\"composeCpu\":";
            writeSamples(rawComposeMs);
            output << "}}";
            std::cout << "Complex UI " << (dirty ? "dirty" : "clean") << " round " << round + 1 << ": " << 40000 / totalMs << " completed offscreen FPS; p95 "
                      << frameMs[37] << " ms; private " << memoryAfter.PrivateUsage << " bytes\n";
        }
        output << "]}";
    }
    output << ']';
    if (retention)
    {
        // Six complete passes through the same 1,000-row model distinguish
        // initial native font/heap caches from growth on repeated data. No
        // working-set trimming or allocator purge may hide retained resources.
        output << ",\"retention\":[";
        const auto started = Clock::now();
        const auto sample  = [&](size_t frame, const char* phase)
        {
            const auto memory = Memory();
            DWORD handles     = 0;
            Check(GetProcessHandleCount(GetCurrentProcess(), &handles) != FALSE, "retention handle count");
            output << "{\"frame\":" << frame << ",\"phase\":\"" << phase << "\",\"elapsedMs\":" << elapsed(started)
                   << ",\"privateBytes\":" << memory.PrivateUsage << ",\"workingSetBytes\":" << memory.WorkingSetSize << ",\"handles\":" << handles
                   << ",\"surfaceBytes\":" << view.GetStatistics().surfaceBytes;
            if (heapDiagnostic)
                WriteHeapDiagnostic(output);
            output << '}';
        };
        sample(0u, "start");
        for (size_t frame = 0u; frame < 6000u; ++frame)
        {
            update(frame);
            Hr(view.Prepare(1280, 720), "retention preparation");
            gpu.Bind();
            Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "retention composition");
            complete();
            // Diagnostic only: match allocation rate in wall-clock time as well
            // as frame count. Never pace production or the measured FPS rounds.
            if (paced)
                std::this_thread::sleep_until(started + std::chrono::milliseconds(20u * (frame + 1u)));
            if ((frame + 1u) % 200u == 0u)
            {
                output << ',';
                sample(frame + 1u, "scroll");
            }
        }
        scene.grid->SetModel(nullptr);
        output << ',';
        sample(6000u, "model-cleared");
        output << ']';
    }
    view.SetVisible(false);
    const auto hidden = view.GetStatistics();
    Check(! view.NeedsAnimation() && ! view.NeedsPreparation(), "complex hidden view requests no work");
    Check(view.Prepare(1280, 720) == S_FALSE, "hidden benchmark preparation skipped");
    Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE, "hidden benchmark composition skipped");
    Check(view.GetStatistics().preparations == hidden.preparations && view.GetStatistics().composites == hidden.composites, "hidden counters unchanged");
    memoryPhases[5] = Memory();
    output << ",\"hiddenPreparations\":0,\"hiddenComposites\":0,\"memoryPhases\":[";
    constexpr std::array names{"entry", "device", "scene", "warm", "capture", "hidden"};
    for (size_t index = 0; index < memoryPhases.size(); ++index)
    {
        if (index)
            output << ',';
        output << "{\"name\":\"" << names[index] << "\",\"privateBytes\":" << memoryPhases[index].PrivateUsage
               << ",\"workingSetBytes\":" << memoryPhases[index].WorkingSetSize << '}';
    }
    output << ']';
    if (retention)
    {
        view.Controls().SetRoot(nullptr);
        view.Detach();
        const auto memory = Memory();
        DWORD handles     = 0;
        Check(GetProcessHandleCount(GetCurrentProcess(), &handles) != FALSE, "detached handle count");
        output << ",\"detached\":{\"privateBytes\":" << memory.PrivateUsage << ",\"workingSetBytes\":" << memory.WorkingSetSize << ",\"handles\":" << handles;
        if (heapDiagnostic)
            WriteHeapDiagnostic(output);
        output << '}';
    }
    output << "}\n";
    output.close();
    Check(bool(output), "benchmark report written");
}
} // namespace ComplexUiBenchmark
