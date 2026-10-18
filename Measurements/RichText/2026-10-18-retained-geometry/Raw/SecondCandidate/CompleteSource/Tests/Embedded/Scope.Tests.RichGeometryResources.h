#pragma once
#include <DxUi/RichTextLayout.h>
static wil::com_ptr<IDWriteFactory> RichGeometryFactory()
{
    wil::com_ptr<IDWriteFactory> factory;
    Hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())), "rich resource DWrite factory");
    return factory;
}
static void TestRichPreparedLayoutClearAllocationBudget(bool trace = false)
{
    using namespace DxUi::RichText;
    const auto factory = RichGeometryFactory();
    Document document(L"# Heading\n**bold** Unicode \U0001f600");
    PreparedLayout layout;
    layout.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), 310.0f);
    const auto generation  = layout.Generation();
    const auto oldCounting = countAllocations;
    const auto oldCalls = allocations, oldBytes = allocationBytes, oldFailure = failAllocationAfter;
    size_t calls = 0u, bytes = 0u;
    if (trace)
    {
        std::fputs("rich layout clear failure-point=0\n", stderr);
        std::fflush(stderr);
    }
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations    = oldCounting;
            allocations         = oldCalls;
            allocationBytes     = oldBytes;
            failAllocationAfter = oldFailure;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        failAllocationAfter           = 0u;
        layout.Clear();
        layout.Clear();
        failAllocationAfter = oldFailure;
        countAllocations    = false;
        calls               = allocations;
        bytes               = allocationBytes;
    }
    Check(! layout.Ready() && layout.Content().source.empty() && layout.Content().text.empty() && layout.Content().runs.empty() &&
              layout.Content().units.empty() && layout.Content().sourceToDisplay.empty() && layout.Height() == 0.0f && layout.Generation() == generation + 2u &&
              calls == 0u && bytes == 0u,
          "prepared layout retirement and empty content never allocate, including failure point zero");
    std::cout << "rich layout clear CppAllocations=" << calls << " CppRequestedBytes=" << bytes << '\n';
}
static void TestRichDocumentGeometryFailureAtomicity(bool trace = false)
{
    using namespace DxUi::RichText;
    const auto factory = RichGeometryFactory();
    std::wstring seed;
    for (size_t line = 0u; line < 96u; ++line)
        seed += L"line **bold** \U0001f600\r\n";
    Document document(seed);
    const auto before   = document.Snapshot();
    const auto revision = document.Revision();
    document.Replace(seed.size() / 2u, seed.size() / 2u + 1u, L"# changed\n", {0u, 0u});
    LayoutOptions options;
    options.widthDip = 310.0f;
    size_t requests  = 0u;
    {
        PreparedDocumentLayout prepared;
        prepared.Prepare(factory.get(), before, revision, ViewMode::Rich, options);
        const auto oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        countAllocations = false;
        requests         = allocations;
    }
    Check(requests > 0u && requests < 2048u, "chunked geometry has a finite observed C++ allocation schedule");
    for (size_t failure = 0u; failure < requests; ++failure)
    {
        if (trace)
        {
            std::fprintf(stderr, "rich geometry failure-point=%zu\n", failure);
            std::fflush(stderr);
        }
        PreparedDocumentLayout prepared;
        prepared.Prepare(factory.get(), before, revision, ViewMode::Rich, options);
        const auto generation = prepared.Generation();
        const auto height     = prepared.Height();
        const auto text       = prepared.DisplayText();
        const auto chunks     = prepared.ChunkCount();
        bool rejected         = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = failure;
            try
            {
                prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && prepared.Ready() && prepared.Generation() == generation && prepared.Revision() == revision && prepared.Height() == height &&
                  prepared.ChunkCount() == chunks && prepared.DisplayText() == text && prepared.SourceMatches(seed),
              "every failed geometry preparation preserves its published source, mapping, geometry and revision");
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        Check(prepared.Ready() && prepared.Revision() == document.Revision() && prepared.SourceMatches(document.Text()),
              "retry after geometry allocation failure publishes the complete current pair");
    }
    std::cout << "rich document geometry allocation-failure sweptRequests=" << requests << '\n';
}
static void TestRichDocumentGeometryUnchangedAllocationBudget()
{
    using namespace DxUi::RichText;
    const auto factory = RichGeometryFactory();
    std::wstring seed;
    for (size_t line = 0u; line < 512u; ++line)
        seed += L"**a\u0301** \U0001f600\r\n";
    Document document(seed);
    LayoutOptions options;
    options.widthDip = 310.0f;
    PreparedDocumentLayout prepared;
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    const auto flat        = Project(document.Snapshot(), document.Revision(), ViewMode::Rich);
    const auto oldCounting = countAllocations;
    const auto oldCalls = allocations, oldBytes = allocationBytes;
    bool correct = true;
    size_t calls = 0u, bytes = 0u;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        for (size_t repeat = 0u; repeat < 4u; ++repeat)
        {
            prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
            correct = correct && prepared.SourceMatches(seed);
            for (size_t offset = 0u; offset <= flat.text.size(); ++offset)
            {
                correct = correct && prepared.ToSource(offset) == flat.ToSource(offset) && prepared.ToSource(offset, true) == flat.ToSource(offset, true) &&
                          prepared.Caret(offset).height > 0.0f;
            }
        }
        countAllocations = false;
        calls            = allocations;
        bytes            = allocationBytes;
    }
    std::cout << "rich geometry unchanged repeats=4 lines=512 CppAllocations=" << calls << " CppRequestedBytes=" << bytes << '\n';
    Check(correct && calls == 0u && bytes == 0u, "unchanged geometry and ACP/caret queries allocate no C++ storage");
}
static void TestRichDocumentGeometryMaximumStorageBudget()
{
    using namespace DxUi::RichText;
    const auto factory = RichGeometryFactory();
    LayoutOptions options;
    options.widthDip = 321.25f;
    for (size_t kind = 0u; kind < 3u; ++kind)
    {
        std::wstring seed;
        if (kind < 2u)
            seed.assign(Document::MaximumUnits, kind ? L'a' : L'\n');
        else
            for (size_t line = 0u; line < Document::MaximumUnits / 2u; ++line)
            {
                seed.push_back(static_cast<wchar_t>(0x1000u + line));
                seed.push_back(L'\n');
            }
        const auto oldCounting = countLiveBytes;
        const auto oldBytes    = liveBytes;
        ptrdiff_t retained = 0, released = 0;
        size_t chunks = 0u;
        {
            const auto restore = wil::scope_exit([=] noexcept
            {
                countLiveBytes = oldCounting;
                liveBytes      = oldBytes;
            });
            liveBytes          = 0;
            countLiveBytes     = true;
            {
                Document document(seed);
                PreparedDocumentLayout prepared;
                prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
                chunks   = prepared.ChunkCount();
                retained = liveBytes;
                prepared.Clear();
            }
            released       = liveBytes;
            countLiveBytes = false;
        }
        std::cout << "rich geometry maximum kind=" << kind << " sourceUnits=65536 chunks=" << chunks << " retainedCppBytes=" << retained
                  << " releasedDelta=" << released << '\n';
        Check(chunks > 0u && chunks <= 128u && retained >= 0 && retained <= 24 * 1024 * 1024 && released == 0,
              "maximum source bounds published geometry to 128 chunks and tracked C++ storage to 24 MiB, releasing every tracked byte");
    }
    // DirectWrite's own private heaps are outside this executable's C++ hooks;
    // the COM graph count is bounded independently, not reported as heap bytes.
}
static void TestEmbeddedRichGeometryReuseAndRetirement(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::wstring seed;
    for (size_t line = 0u; line < 512u; ++line)
        seed += L"line **bold** a\u0301 \U0001f600\r\n";
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "chunk reuse shared graphics");
    EmbeddedHost first, second;
    Hr(first.Attach(graphics), "chunk reuse first view");
    Hr(second.Attach(graphics), "chunk reuse second view");
    auto owned      = std::make_unique<RichTextEditor>(seed);
    auto otherOwned = std::make_unique<RichTextEditor>(seed);
    auto* editor    = owned.get();
    auto* other     = otherOwned.get();
    first.Controls().SetRoot(std::move(owned));
    second.Controls().SetRoot(std::move(otherOwned));
    Hr(first.Prepare(480, 240), "chunk reuse initial frame");
    Hr(second.Prepare(320, 160, 144), "chunk reuse independent variant");
    const auto initial     = editor->DebugGetState();
    const auto independent = other->DebugGetState();
    Check(initial.retainedGeometryChunks > 1u && initial.retainedGeometryChunks <= 128u, "embedded source really uses bounded multiple chunks");
    editor->SetSelection({seed.size() / 2u, seed.size() / 2u + 1u});
    Check(editor->ReplaceSelection(L"x") && ! editor->DebugGetState().layoutReady &&
              editor->DebugGetState().retainedGeometryChunks == initial.retainedGeometryChunks,
          "embedded source invalidation hides stale geometry while retaining its bounded graph");
    Hr(first.Prepare(480, 240), "chunk reuse changed frame");
    Check(editor->DebugGetState().reusedGeometryChunks > 0u && editor->DebugGetState().preparedSourceUnits <= 2048u &&
              other->DebugGetState().layoutGeneration == independent.layoutGeneration && other->GetText() == seed,
          "embedded preparation reuses unchanged geometry without touching another variant");
    const auto statistics  = first.GetStatistics();
    const auto oldCounting = countAllocations;
    const auto oldCalls = allocations, oldBytes = allocationBytes;
    size_t calls = 0u, bytes = 0u;
    gpu.Bind();
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        for (size_t frame = 0u; frame < 120u; ++frame)
            Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "chunked cached composition");
        countAllocations = false;
        calls            = allocations;
        bytes            = allocationBytes;
    }
    Check(calls == 0u && bytes == 0u && first.GetStatistics().preparations == statistics.preparations &&
              first.GetStatistics().surfaceAllocations == statistics.surfaceAllocations,
          "chunked cached composition performs no C++ allocation or preparation");
    Hr(first.Prepare(320, 240), "chunk geometry width variant");
    Check(editor->DebugGetState().reusedGeometryChunks == 0u, "width change cannot reuse incompatible geometry");
    first.SetVisible(false);
    Check(! editor->DebugGetState().layoutReady && editor->DebugGetState().retainedGeometryChunks == 0u && ! first.NeedsPreparation(),
          "hidden embedded view retires every geometry chunk without scheduling work");
    first.SetVisible(true);
    Hr(first.Prepare(480, 240), "resumed chunk geometry");
    Check(editor->DebugGetState().reusedGeometryChunks == 0u && editor->Undo() && editor->GetText() == seed,
          "resuming rebuilds resources while retaining independent source history");
}
