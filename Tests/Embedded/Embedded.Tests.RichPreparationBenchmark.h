#pragma once
#include "GraphicsFixture.h"
#include <DxUi/Embedded.h>
#include <DxUi/RichImage.h>
#include <chrono>
#include <iostream>
#include <psapi.h>
#include <wil/resource.h>
#pragma comment(lib, "psapi.lib")

// Read-only common-API benchmark. The same source is linked independently to
// baseline and candidate libraries; it owns no HWND, files, timers or workers.
inline int RunRichPreparationBenchmark()
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized))
        return 1;
    const auto uninitialize = wil::scope_exit([]() noexcept { CoUninitialize(); });
    GraphicsFixture gpu;
    gpu.width  = 800u;
    gpu.height = 600u;
    if (FAILED(gpu.Create()))
        return 2;
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    if (FAILED(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics)))
        return 3;
    DxUi::EmbeddedHost view;
    if (FAILED(view.Attach(graphics)))
        return 4;
    std::wstring source;
    for (unsigned i = 0u; i < 1000u; ++i)
    {
        source += L"Paragraph ";
        source += std::to_wstring(i);
        source += L" with **styled** text and a link.\n\n";
    }
    source += L"- one\n- two\n\n| Name | Value |\n| --- | --- |\n| alpha | beta |\n\n";
    source += L"![one](one.png) ![two](two.png)\n\n$x+y$ and $a^2$ and $z_1$\n\n";
    source += L"```mermaid\ngraph LR;A[Draft]-->B[Ready]\n```\n";
    auto root    = std::make_unique<DxUi::Panel>();
    auto* editor = root->AddChild<DxUi::RichTextEditor>(source);
    editor->SetBounds(D2D1::RectF(0.f, 0.f, 800.f, 600.f));
    editor->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    editor->SetDocumentFilePath(L"C:\\fixture\\document.md");
    view.Controls().SetRoot(std::move(root));
    const auto settle = [&]() -> std::size_t
    {
        std::size_t turns{};
        do
        {
            if (++turns > 20000u || FAILED(view.Prepare(800u, 600u)))
                return 0u;
        } while (view.NeedsPreparation());
        return turns;
    };
    const auto started = std::chrono::steady_clock::now();
    const auto initial = settle();
    if (! initial)
        return 5;
    std::vector<DxUi::RichText::RichImageRequest> jobs(0u);
    editor->TakePendingImageRequests(jobs);
    std::vector<std::byte> pixels(16u * 16u * 4u, std::byte{0xff});
    std::shared_ptr<const DxUi::RichText::RichImage> image;
    if (FAILED(DxUi::RichText::RichImage::CreatePbgra(16u, 16u, pixels, image)))
        return 6;
    std::size_t accepted{};
    const auto beforeBurst = editor->DebugGetState();
    for (const auto& job : jobs)
        accepted += editor->CompleteImageRequest(job, image) ? 1u : 0u;
    const auto burst = settle();
    if (! burst)
        return 7;
    const auto afterBurst = editor->DebugGetState();
    editor->SetSelection({10u, 10u});
    if (! editor->ReplaceSelection(L"x"))
        return 8;
    const auto beforeEdit = editor->DebugGetState();
    const auto edited     = settle();
    if (! edited)
        return 9;
    const auto afterEdit = editor->DebugGetState();
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    if (! GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)))
        return 10;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    std::cout << "{\"paragraphs\":1000,\"source_units\":" << source.size() << ",\"initial_turns\":" << initial << ",\"image_jobs\":" << jobs.size()
              << ",\"image_completions\":" << accepted << ",\"burst_turns\":" << burst
              << ",\"burst_layout_advances\":" << afterBurst.preparationAdvances - beforeBurst.preparationAdvances << ",\"edit_turns\":" << edited
              << ",\"edit_layout_advances\":" << afterEdit.preparationAdvances - beforeEdit.preparationAdvances
              << ",\"edit_semantic_advances\":" << afterEdit.semanticPreparationAdvances - beforeEdit.semanticPreparationAdvances
              << ",\"retained_chunks\":" << afterEdit.retainedGeometryChunks << ",\"private_bytes\":" << memory.PrivateUsage << ",\"elapsed_ms\":" << elapsed
              << "}\n";
    return 0;
}
