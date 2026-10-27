#pragma once
#include "../Support/Support.Tests.RichSemanticGeometry.h"

static void TestRichSemanticEditorQueryFailureBudgets(GraphicsFixture& gpu, bool trace = false)
{
    using namespace DxUi;
    const std::wstring source =
        L"**alpha**\r\n[link](local) &amp; ![alt](image)\r\n\n9. task <b>bold</b>\n10. next\n\n<p><a href='#x'>HTML</a><br/>&fjlig;</p>\n";
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "query failure borrowed graphics");
    for (const bool semantic : {false, true})
    {
        EmbeddedHost view;
        Hr(view.Attach(graphics), "query failure embedded owner");
        auto root    = std::make_unique<RichTextEditor>(source);
        auto* editor = root.get();
        editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
        editor->SetMarkdownRenderingEnabled(semantic);
        view.Controls().SetRoot(std::move(root));
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "query failure current geometry");
        const auto revision   = editor->Revision();
        const auto generation = editor->DebugGetState().layoutGeneration;
        for (unsigned operation = 0u; operation != 2u; ++operation)
        {
            const auto query = [&]
            {
                return operation == 0u ? editor->TryGetTextInputRangeRects(view.Controls(), 0u, source.size()).has_value()
                                       : editor->GetSourceVisualLineRanges(view.Controls()).has_value();
            };
            Check(query(), "query failure fixture starts with available glyph/line geometry");
            size_t requests = 0u;
            {
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
                const bool available          = query();
                countAllocations              = false;
                requests                      = allocations;
                Check(available && requests > 0u && requests < 256u, "query allocation schedule is finite and independently observed");
            }
            for (size_t point = 0u; point < requests; ++point)
            {
                if (trace)
                {
                    std::fprintf(
                        stderr, "semantic query engine=%u operation=%u allocation failure point=%zu of %zu\n", semantic ? 1u : 0u, operation, point, requests);
                    std::fflush(stderr);
                }
                bool available = false;
                {
                    const auto previous = failAllocationAfter;
                    const auto restore  = wil::scope_exit([=] noexcept { failAllocationAfter = previous; });
                    failAllocationAfter = point;
                    available           = query();
                }
                Check(! available && editor->Revision() == revision && editor->DebugGetState().layoutReady &&
                          editor->DebugGetState().layoutGeneration == generation && editor->GetText() == source && ! editor->CanUndo(),
                      "each text-service query allocation failure reports unavailable without source/history/geometry mutation");
                Check(query(), "text-service geometry query succeeds after every injected failure");
            }
            std::cout << "rich semantic query engine=" << semantic << " operation=" << operation << " sweptRequests=" << requests << '\n';
        }
    }
}

static void TestRichSemanticGeometryFailureBudgets(
    bool trace = false, bool literalFootnotes = false, bool renderedFootnotes = false, bool spacing = false, bool subtle = false)
{
    using namespace DxUi::RichText;
    using DxUi::RichText::Detail::PreparedSemanticGeometry;
    auto factory        = RichSemanticGeometryTests::Factory();
    const auto previous = RichSemanticDocumentTests::Build(L"previous\n");
    auto candidate =
        renderedFootnotes  ? RichSemanticDocumentTests::Build(
                                 L"[^unused]: literal\n[^z]: Z body\n[^a]: first\n\n    second\n\n[^empty]:\n\nmain[^a] and[^z] again[^a] empty[^empty]\n",
                                 MarkdownProfile::RichMarkdown)
        : literalFootnotes ? RichSemanticDocumentTests::Build(L"[^n]: first\n\n    > - second\n    >\n    >   third\n\nAfter\n", MarkdownProfile::RichMarkdown)
                           : RichSemanticDocumentTests::Build(
                                 L"# **Heading**\n\n| left | right |\n| :--- | ---: |\n| a |\n\n> - [x] [label](local) and ![alt](image)\n>\n- \n\n9. "
                                 L"<b>native</b>\n10. next\n\n<p><a href='#x'>HTML</a><br/>&fjlig;</p>\n\n<p><b></b></p>\n");
    LayoutOptions options;
    options.widthDip = 240.0f;
    if (subtle)
    {
        options.markdownPresentation = MarkdownPresentationStyle::Subtle;
        candidate =
            RichSemanticDocumentTests::Build(std::wstring(candidate->Map().Source()) + L"\n| another | wider column |\n| --- | --- |\n| second | table |\n");
    }
    if (spacing)
    {
        // The native-height opt-in also sweeps the separately budgeted empty
        // body measurement and optional parent-first item metadata allocation.
        options.paragraphSpacingAfterLines = 1.0f;
        options.listItemSpacingDip         = 10.0f;
    }
    size_t requests = 0u;
    {
        PreparedSemanticGeometry geometry;
        const auto oldCount = countAllocations;
        const auto oldCalls = allocations;
        const auto restore  = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCount;
            allocations      = oldCalls;
        });
        allocations         = 0u;
        countAllocations    = true;
        geometry.Begin(factory.get(), candidate, options);
        RichSemanticGeometryTests::Complete(geometry);
        countAllocations = false;
        requests         = allocations;
    }
    Check(requests > 0u && requests < 4096u, "semantic geometry has an independently observed finite owned allocation schedule");
    for (size_t point = 0u; point < requests; ++point)
    {
        if (trace)
        {
            std::fprintf(stderr, "semantic geometry allocation failure point=%zu of %zu\n", point, requests);
            std::fflush(stderr);
        }
        PreparedSemanticGeometry geometry;
        geometry.Begin(factory.get(), previous, options);
        RichSemanticGeometryTests::Complete(geometry);
        const auto generation = geometry.Generation();
        bool rejected         = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = point;
            try
            {
                geometry.Begin(factory.get(), candidate, options);
                RichSemanticGeometryTests::Complete(geometry);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && geometry.Ready() && ! geometry.Pending() && geometry.Generation() == generation && &geometry.Document() == previous.get(),
              "every geometry allocation failure preserves the complete previous source/layout publication and retires staging");
        geometry.Begin(factory.get(), candidate, options);
        RichSemanticGeometryTests::Complete(geometry);
        Check(&geometry.Document() == candidate.get(), "explicit semantic geometry retry succeeds after each failed allocation");
    }
    PreparedSemanticGeometry geometry;
    geometry.Begin(factory.get(), candidate, options);
    RichSemanticGeometryTests::Complete(geometry);
    const auto oldCalls = allocations;
    countAllocations    = true;
    for (size_t repeat = 0u; repeat < 8u; ++repeat)
    {
        geometry.Begin(factory.get(), candidate, options);
        geometry.Caret(0u);
        geometry.HitTest(4.0f, 4.0f);
        geometry.SourceCaret(0u);
        const auto sourceHit = geometry.HitTestSource(4.0f, 4.0f);
        geometry.AdjacentSourceLine(sourceHit.position, sourceHit.site, 4.0f, true);
        geometry.AdjacentSourceLine(sourceHit.position, sourceHit.site, 4.0f, false);
        geometry.StepSource(sourceHit.position, sourceHit.site, true);
        geometry.StepSource(sourceHit.position, sourceHit.site, false);
        geometry.SourceLineBoundary(sourceHit.position, sourceHit.site, true);
        geometry.VisibleSourceRange(0.0f, geometry.Height());
        for (const auto& line : candidate->Syntax()->EmptyLines())
        {
            const auto caret = geometry.SourceCaret(line.insertion);
            const auto hit   = geometry.HitTestSource(caret.x, caret.y + caret.height * .5f);
            geometry.SourceCaret(hit.position, hit.site);
            geometry.AdjacentSourceLine(hit.position, hit.site, caret.x, true);
            geometry.AdjacentSourceLine(hit.position, hit.site, caret.x, false);
        }
        geometry.SourceLines();
    }
    geometry.Clear();
    countAllocations = false;
    Check(allocations == oldCalls, "semantic geometry coalescing, scalar geometry queries and retirement allocate no owned C++ storage");
    std::cout << "rich semantic geometry literalFootnotes=" << literalFootnotes << " renderedFootnotes=" << renderedFootnotes << " spacing=" << spacing
              << " completeFailures=" << requests << " idle/retireAllocations=0\n";
    if (! spacing)
        TestRichSemanticGeometryFailureBudgets(trace, literalFootnotes, renderedFootnotes, true);
}
