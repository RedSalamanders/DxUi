#pragma once
#include "Support.Tests.RichSemanticGeometry.h"

__declspec(noinline) static void RichSemanticQueryFaultOrigin(DxUi::RichTextEditor& editor, DxUi::EmbeddedHost& view, size_t length, unsigned operation)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    if (operation == 0u)
        (void)editor.TryGetTextInputRangeRects(view.Controls(), 0u, length);
    else
        (void)editor.GetSourceVisualLineRanges(view.Controls());
}
static void TestRichSemanticEditorQueryFailureBudgets(GraphicsFixture& gpu, bool trace = false, unsigned fatalScenario = 0u)
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
            if (fatalScenario == 1u + (semantic ? 2u : 0u) + operation)
            {
                RichSemanticQueryFaultOrigin(*editor, view, source.size(), operation);
                return;
            }
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
            if (fatalScenario == 0u)
            {
                const wchar_t* child = semantic ? (operation ? L"embedded-semantic-line-query" : L"embedded-semantic-range-query")
                                                : (operation ? L"embedded-source-line-query" : L"embedded-source-range-query");
                Check(DxUiFailureTests::ExpectFatalChild(child), "text-service geometry query allocation terminates in its owned child");
            }
            Check(query() && editor->Revision() == revision && editor->DebugGetState().layoutReady && editor->DebugGetState().layoutGeneration == generation &&
                      editor->GetText() == source && ! editor->CanUndo(),
                  "text-service geometry query preserves source, history and geometry");
            if (trace)
                std::cout << "rich semantic query engine=" << semantic << " operation=" << operation << " requests=" << requests << '\n';
        }
    }
}

__declspec(noinline) static void RichSemanticGeometryFaultOrigin(DxUi::RichText::Detail::PreparedSemanticGeometry& geometry,
                                                                 IDWriteFactory* factory,
                                                                 const std::shared_ptr<const DxUi::RichText::SemanticDocument>& document,
                                                                 const DxUi::RichText::LayoutOptions& options)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    geometry.Begin(factory, document, options);
    RichSemanticGeometryTests::Complete(geometry);
}
static void TestRichSemanticGeometryFailureBudgets(
    bool trace = false, bool literalFootnotes = false, bool renderedFootnotes = false, bool spacing = false, bool subtle = false, bool fatalChild = false)
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
    if (fatalChild)
    {
        PreparedSemanticGeometry prior;
        prior.Begin(factory.get(), previous, options);
        RichSemanticGeometryTests::Complete(prior);
        RichSemanticGeometryFaultOrigin(prior, factory.get(), candidate, options);
        return;
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
    {
        PreparedSemanticGeometry prior;
        prior.Begin(factory.get(), previous, options);
        RichSemanticGeometryTests::Complete(prior);
        const wchar_t* child = subtle              ? (spacing ? L"embedded-semantic-geometry-subtle-spacing" : L"embedded-semantic-geometry-subtle")
                               : literalFootnotes  ? (spacing ? L"embedded-semantic-geometry-literal-spacing" : L"embedded-semantic-geometry-literal")
                               : renderedFootnotes ? (spacing ? L"embedded-semantic-geometry-rendered-spacing" : L"embedded-semantic-geometry-rendered")
                                                   : (spacing ? L"embedded-semantic-geometry-spacing" : L"embedded-semantic-geometry-default");
        Check(DxUiFailureTests::ExpectFatalChild(child), "semantic geometry allocation terminates in its owned child");
        prior.Begin(factory.get(), candidate, options);
        RichSemanticGeometryTests::Complete(prior);
        Check(&prior.Document() == candidate.get(), "semantic geometry publishes the complete candidate");
    }
    PreparedSemanticGeometry geometry;
    geometry.Begin(factory.get(), candidate, options);
    RichSemanticGeometryTests::Complete(geometry);
    {
        Hit hit{};
        hit.position = 19u;
        Check(geometry.TryHitTest((std::numeric_limits<float>::quiet_NaN)(), 0.0f, hit) == E_INVALIDARG && hit.position == 19u,
              "invalid hit coordinates preserve the output");
        CaretGeometry caret{};
        caret.x = 17.0f;
        Check(geometry.TryCaret(candidate->Map().Display().size() + 1u, caret) == E_INVALIDARG && caret.x == 17.0f,
              "invalid display caret preserves the output");
        std::vector<D2D1_RECT_F> rectangles{{1.0f, 2.0f, 3.0f, 4.0f}};
        Check(geometry.TryRange(1u, 0u, rectangles) == E_INVALIDARG && rectangles.size() == 1u && rectangles[0].left == 1.0f,
              "invalid display range preserves the output");
        Check(geometry.TrySourceRange({1u, 0u}, rectangles) == E_INVALIDARG && rectangles.size() == 1u && rectangles[0].left == 1.0f,
              "invalid source range preserves the output");
        std::optional<D2D1_RECT_F> bounds{D2D1_RECT_F{1.0f, 2.0f, 3.0f, 4.0f}};
        Check(geometry.TryElementBounds((std::numeric_limits<size_t>::max)(), bounds) == E_INVALIDARG && bounds && bounds->left == 1.0f,
              "invalid element index preserves the output");
        std::pair<size_t, size_t> viewport{3u, 7u};
        Check(geometry.TryVisibleRange(2.0f, 1.0f, viewport) == E_INVALIDARG && viewport == std::pair<size_t, size_t>{3u, 7u},
              "invalid viewport preserves the output");
    }
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
        TestRichSemanticGeometryFailureBudgets(trace, literalFootnotes, renderedFootnotes, true, subtle);
}
