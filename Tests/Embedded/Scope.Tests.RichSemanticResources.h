#pragma once
#include "../Support/Scope.Tests.RichSemantic.h"

// Included after this process's isolated allocation counters. Do not instrument
// the canonical benchmark or Windows/driver allocations with this fixture.
static void TestRichMarkdownCommandHostFailureAtomicity(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "command host failure graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "command host failure owner");
    std::wstring source(L"- [ ] ");
    source.append(128u, L'x');
    auto root    = std::make_unique<RichTextEditor>(source);
    auto* editor = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(root));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "command host failure preparation");
    auto& host = view.Controls();
    host.SetFocusControl(editor, false);
    editor->SetSelection({8u, 8u});
    Check(host.HasActiveNativeTextInputSession(), "embedded/native shared text-service session is active");
    NativeTextInputState available{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
    Check(host.TryReadNativeTextInputState(editor, available) && available.text == source, "command guard starts from a current text-service cache");
    NativeTextInputState unavailable{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
    bool read = true;
    {
        const auto old      = failAllocationAfter;
        const auto restore  = wil::scope_exit([=] noexcept { failAllocationAfter = old; });
        failAllocationAfter = 0u;
        read                = host.TryReadNativeTextInputState(editor, unavailable);
    }
    Check(! read && host.TryReadNativeTextInputState(editor, available) && available.text == source,
          "cache-copy allocation failure returns unavailable without terminating or changing the cache");
    std::size_t constructorRequests = 0u;
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
        NativeTextInputState local{.text = std::wstring(0u, L' '), .compositionClauseBoundaries = std::vector<size_t>(0u)};
        countAllocations    = false;
        constructorRequests = allocations;
    }
    MarkdownCommandRequest request;
    request.command              = MarkdownCommand::ToggleTask;
    const auto original          = editor->Snapshot();
    const auto revision          = editor->Revision();
    const auto selection         = editor->GetSelection();
    const auto plan              = BuildMarkdownCommand(original, revision, editor->GetMarkdownSyntax(), selection, request);
    MarkdownCommandStatus result = MarkdownCommandStatus::Changed;
    {
        const auto old      = failAllocationAfter;
        const auto restore  = wil::scope_exit([=] noexcept { failAllocationAfter = old; });
        failAllocationAfter = constructorRequests;
        result              = editor->ApplyMarkdownCommand(plan);
    }
    Check(result == MarkdownCommandStatus::Unavailable && editor->Snapshot().IsSameRevision(original) && editor->Revision() == revision &&
              editor->GetSelection() == selection && ! editor->CanUndo(),
          "structural command fails closed when current composition state cannot be read");
    Check(editor->ApplyMarkdownCommand(plan) == MarkdownCommandStatus::Changed && editor->Undo() && editor->Snapshot().IsSameRevision(original),
          "guard failure can retry once and undo exactly");
    std::cout << "rich command host cache failure: guardCtorRequests=" << constructorRequests << " sourcePreserved=1 retryUndo=1\n";
}
static void TestRichMarkdownCommandFailureAtomicity()
{
    using namespace DxUi::RichText;
    const auto countRequests = [](auto&& operation)
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
        operation();
        countAllocations = false;
        return allocations;
    };
    for (unsigned scenario = 0u; scenario < 12u; ++scenario)
    {
        std::wstring_view spelling = scenario < 2u ? L"- a\n- b\n" : scenario < 7u ? L"**abc** &copy;" : L"a | b\n- | -\nx | y";
        MarkdownCommandRequest request;
        request.command     = scenario == 0u   ? MarkdownCommand::IndentList
                              : scenario == 1u ? MarkdownCommand::SetList
                              : scenario == 2u ? MarkdownCommand::SetHeading
                              : scenario == 3u ? MarkdownCommand::ToggleStyle
                              : scenario == 4u ? MarkdownCommand::SetLink
                              : scenario == 5u ? MarkdownCommand::ToggleTask
                              : scenario == 7u ? MarkdownCommand::InsertTableColumn
                              : scenario == 8u ? MarkdownCommand::DeleteTableRow
                                               : MarkdownCommand::SetTableCell;
        request.listKind    = MarkdownListKind::Ordered;
        request.style       = MarkdownTextStyle::Code;
        request.destination = L"target";
        request.text        = L"new";
        Selection selection = scenario < 2u    ? Selection{6u, 6u}
                              : scenario == 3u ? Selection{0u, 14u}
                              : scenario >= 7u ? Selection{16u, 16u}
                                               : Selection{2u, 5u};
        if (scenario >= 10u)
        {
            spelling        = L"<https://example.invalid/?x&copy;>";
            selection       = {4u, 4u};
            request.command = scenario == 10u ? MarkdownCommand::RemoveLink : MarkdownCommand::SetLink;
        }
        const auto seed = [&](Document& document)
        {
            document.Replace(0u, 0u, L"x", {});
            document.Undo();
            document.SetSelection(selection);
        };
        const auto invoke = [&](Document& document, const auto& syntax)
        {
            if (scenario == 6u)
            {
                const auto plan = BuildMarkdownDeletion(document.Snapshot(), document.Revision(), syntax, selection, std::array{MarkdownRange{2u, 5u}});
                ApplyMarkdownCommand(document, plan);
            }
            else
            {
                const auto plan = BuildMarkdownCommand(document.Snapshot(), document.Revision(), syntax, selection, request);
                ApplyMarkdownCommand(document, plan);
            }
        };
        Document measure(spelling);
        seed(measure);
        const auto analysis = RichCommandTests::Parse(measure);
        const auto requests = countRequests([&] { invoke(measure, analysis); });
        Check(requests > 0u && requests < 2048u, "bounded structural staging and transaction allocations");
        for (std::size_t failure = 0u; failure < requests; ++failure)
        {
            Document document(spelling);
            seed(document);
            const auto syntax   = RichCommandTests::Parse(document);
            const auto snapshot = document.Snapshot();
            const auto revision = document.Revision();
            const auto undo = document.CanUndo(), redo = document.CanRedo();
            bool rejected = false;
            {
                const auto oldFailure = failAllocationAfter;
                const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
                failAllocationAfter   = failure;
                try
                {
                    invoke(document, syntax);
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && document.Snapshot().IsSameRevision(snapshot) && document.Revision() == revision && document.GetSelection() == selection &&
                      document.CanUndo() == undo && document.CanRedo() == redo && syntax->Source() == spelling,
                  "every structural allocation failure preserves source, selection, undo/redo and analysis");
        }
        std::cout << "rich command allocation scenario=" << scenario << " sweptRequests=" << requests << '\n';
    }
}
static void TestRichSemanticFailureAndStorageBudgets()
{
    using namespace DxUi::RichText;
    const std::wstring source  = L"**alpha** &fjlig; ![image](local) omega";
    const std::wstring display = L"omega alpha fj [image] alpha";
    const auto omega = source.find(L"omega"), entity = source.find(L"&fjlig;"), image = source.find(L"![");
    const std::array spans{DisplaySpan{{omega, source.size()}, {0u, 5u}},
                           DisplaySpan{{2u, 2u}, {5u, 6u}, DisplaySpanKind::Generated, false},
                           DisplaySpan{{2u, 7u}, {6u, 11u}},
                           DisplaySpan{{entity, entity}, {11u, 12u}, DisplaySpanKind::Generated, false},
                           DisplaySpan{{entity, entity + 7u}, {12u, 14u}, DisplaySpanKind::Transformed},
                           DisplaySpan{{image, image}, {14u, 15u}, DisplaySpanKind::Generated, false},
                           DisplaySpan{{image, omega - 1u}, {15u, 22u}, DisplaySpanKind::Object},
                           DisplaySpan{{7u, 7u}, {22u, 23u}, DisplaySpanKind::Generated, false},
                           DisplaySpan{{2u, 7u}, {23u, 28u}, DisplaySpanKind::Source, false}};
    const auto map           = SourceDisplayMap::Build(source, display, spans);
    const auto countRequests = [](auto&& operation)
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
        operation();
        countAllocations = false;
        return allocations;
    };
    size_t swept = 0u;
    for (unsigned operation = 0u; operation < 4u; ++operation)
    {
        const auto invoke = [&]
        {
            if (operation == 0u)
                static_cast<void>(SourceDisplayMap::Build(source, display, spans));
            else if (operation == 1u)
                static_cast<void>(map->DisplayRanges({0u, source.size()}));
            else if (operation == 2u)
                static_cast<void>(map->SourceRanges({0u, display.size()}));
        };
        Document document(source);
        document.Replace(2u, 3u, L"A", {3u, 2u});
        document.Undo();
        const auto snapshot  = document.Snapshot();
        const auto revision  = document.Revision();
        const auto selection = document.GetSelection();
        const auto undo = document.CanUndo(), redo = document.CanRedo();
        const std::array edits{SourceEdit{{2u, 7u}, L"changed"}, SourceEdit{{omega, source.size()}, L"end"}};
        size_t requests = 0u;
        if (operation < 3u)
            requests = countRequests(invoke);
        else
        {
            Document measure(source);
            measure.Replace(2u, 3u, L"A", {3u, 2u});
            measure.Undo();
            const auto expected         = measure.Snapshot();
            const auto expectedRevision = measure.Revision();
            requests                    = countRequests([&] { ApplySourceEdits(measure, expected, expectedRevision, edits, {9u, 2u}); });
        }
        Check(requests > 0u && requests < 256u, "finite semantic map/range/transaction allocation schedule");
        for (size_t failure = 0u; failure < requests; ++failure)
        {
            bool rejected = false;
            {
                const auto previousFailure = failAllocationAfter;
                const auto restore         = wil::scope_exit([=] noexcept { failAllocationAfter = previousFailure; });
                failAllocationAfter        = failure;
                try
                {
                    if (operation < 3u)
                        invoke();
                    else
                        ApplySourceEdits(document, snapshot, revision, edits, {9u, 2u});
                }
                catch (const std::bad_alloc&)
                {
                    rejected = true;
                }
            }
            Check(rejected && map->Source() == source && map->Display() == display && document.Snapshot().IsSameRevision(snapshot) &&
                      document.Revision() == revision && document.GetSelection() == selection && document.CanUndo() == undo && document.CanRedo() == redo,
                  "each semantic allocation failure preserves immutable mapping and complete edit state");
        }
        if (operation == 3u)
        {
            Check(ApplySourceEdits(document, snapshot, revision, edits, {9u, 2u}), "explicit transaction retry succeeds");
            Check(document.Undo() && document.Snapshot().IsSameRevision(snapshot), "transaction retry is one exact undo");
        }
        swept += requests;
        std::cout << "rich semantic allocation operation=" << operation << " sweptRequests=" << requests << '\n';
    }
    size_t checksum          = 0u;
    const auto queryRequests = countRequests([&]
    {
        for (size_t repeat = 0u; repeat < 1024u; ++repeat)
        {
            checksum += map->ToDisplay(3u) + map->ToSource(13u, MappingAffinity::Trailing);
            checksum += map->SnapDisplayBoundary(13u, MappingAffinity::Leading);
        }
    });
    Check(queryRequests == 0u && checksum > 0u, "semantic scalar coordinate queries allocate nothing");
    const std::wstring maximum(Document::MaximumUnits, L'x');
    const std::wstring doubled(SourceDisplayMap::MaximumDisplayUnits, L'x');
    std::vector<DisplaySpan> maximumSpans(0u);
    maximumSpans.reserve(SourceDisplayMap::MaximumSpans);
    // 32K source-backed two-unit spans followed by exact repeated occurrences:
    // simultaneously exercises both maximum display and maximum span admission.
    for (size_t occurrence = 0u; occurrence < 2u; ++occurrence)
        for (size_t offset = 0u; offset < maximum.size(); offset += 2u)
            maximumSpans.push_back({{offset, offset + 2u},
                                    {occurrence * maximum.size() + offset, occurrence * maximum.size() + offset + 2u},
                                    DisplaySpanKind::Source,
                                    occurrence == 0u});
    ptrdiff_t retained = 0, released = 0;
    {
        const auto previousCounting = countLiveBytes;
        const auto previousBytes    = liveBytes;
        const auto restore          = wil::scope_exit([=] noexcept
        {
            countLiveBytes = previousCounting;
            liveBytes      = previousBytes;
        });
        countLiveBytes              = true;
        liveBytes                   = 0;
        {
            const auto maximumMap = SourceDisplayMap::Build(maximum, doubled, maximumSpans);
            retained              = liveBytes;
            Check(maximumMap->ToDisplay(maximum.size()) == maximum.size() && maximumMap->ToSource(doubled.size()) == maximum.size(),
                  "maximum repetitions retain primary and reverse end coordinates");
        }
        released       = liveBytes;
        countLiveBytes = false;
    }
    Check(retained > 0 && retained <= 8 * 1024 * 1024 && released == 0, "maximum semantic map stays within eight MiB and releases all owned C++ storage");
    std::cout << "rich semantic map sweptRequests=" << swept << " coordinateQueryAllocations=" << queryRequests << " maximumSourceUnits=" << maximum.size()
              << " maximumDisplayUnits=" << doubled.size() << " spans=" << maximumSpans.size() << " retainedCppBytes=" << retained
              << " releasedDelta=" << released << '\n';
}
