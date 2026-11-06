#pragma once
#include "Support.Tests.RichSemantic.h"
#include <thread>

// Included after this process's isolated allocation counters. Do not instrument
// the canonical benchmark or Windows/driver allocations with this fixture.
static LRESULT CALLBACK SemanticDispatchWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* host = reinterpret_cast<DxUi::ControlHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        host = static_cast<DxUi::ControlHost*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(host));
    }
    if (host)
    {
        bool handled      = false;
        const auto result = host->HandleMessage(hwnd, message, wParam, lParam, handled);
        if (handled)
            return result;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
__declspec(noinline) static HRESULT RichNativeToggleFaultOrigin(IToggleProvider* toggle, std::size_t failure)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = failure;
    const auto result   = toggle->Toggle();
    failAllocationAfter = std::numeric_limits<std::size_t>::max();
    return result;
}
static void TestRichSemanticNativeDispatchFailureAtomicity(bool fatalChild = false)
{
    using namespace DxUi;
    const auto instance        = GetModuleHandleW(nullptr);
    constexpr PCWSTR className = L"DxUi.Tests.SemanticDispatch";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc   = SemanticDispatchWindowProc;
    windowClass.hInstance     = instance;
    windowClass.lpszClassName = className;
    Check(RegisterClassW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "register nonactivating native dispatch fixture");
    ControlHost host;
    wil::unique_hwnd window(
        CreateWindowExW(WS_EX_NOACTIVATE, className, L"Semantic dispatch", WS_OVERLAPPED, -32000, -32000, 480, 240, nullptr, nullptr, instance, &host));
    Check(bool(window) && host.Attach(window.get()), "attach hidden native dispatch owner without desktop focus");
    const auto detach         = wil::scope_exit([&] { host.Detach(); });
    const std::wstring source = L"- [ ] task\r\n";
    auto root                 = std::make_unique<RichTextEditor>(source);
    auto* editor              = root.get();
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    host.SetRoot(std::move(root));
    const auto prepare = [&]
    {
        for (std::size_t turn = 0u; turn < 1024u; ++turn)
        {
            Check(host.RenderInitialFrameForShow(), "prepare native dispatch semantics");
            if (! host.HasPendingContentPreparation())
                return;
        }
        Check(false, "native dispatch preparation completes in bounded turns");
    };
    prepare();
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    provider.attach(CreateWindowHostAccessibilityProvider(window.get()));
    Check(bool(provider), "native dispatch application provider");
    wil::com_ptr_nothrow<IRawElementProviderFragment> document;
    Hr(provider.query_to(document.put()), "native dispatch document fragment");
    const auto findToggle = [&](auto&& self, IRawElementProviderFragment& fragment) -> wil::com_ptr_nothrow<IToggleProvider>
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        Hr(fragment.QueryInterface(IID_PPV_ARGS(simple.put())), "native dispatch part");
        wil::com_ptr_nothrow<IUnknown> pattern;
        Hr(simple->GetPatternProvider(UIA_TogglePatternId, pattern.put()), "native dispatch task pattern");
        wil::com_ptr_nothrow<IToggleProvider> toggle;
        if (pattern)
        {
            Hr(pattern.query_to(toggle.put()), "native dispatch Toggle interface");
            return toggle;
        }
        wil::com_ptr_nothrow<IRawElementProviderFragment> child;
        Hr(fragment.Navigate(NavigateDirection_FirstChild, child.put()), "native dispatch first child");
        while (child)
        {
            toggle = self(self, *child.get());
            if (toggle)
                return toggle;
            wil::com_ptr_nothrow<IRawElementProviderFragment> next;
            Hr(child->Navigate(NavigateDirection_NextSibling, next.put()), "native dispatch next sibling");
            child = std::move(next);
        }
        return toggle;
    };
    const HWND previousFocus = GetFocus();
    const auto invoke        = [&](std::size_t failure)
    {
        // Independent source/history/publication for every fault position.
        editor->SetText(L"reset");
        editor->SetText(source);
        prepare();
        auto toggle = findToggle(findToggle, *document.get());
        Check(bool(toggle), "native dispatch finds current semantic task");
        wil::unique_event_nothrow completed;
        Hr(completed.create(wil::EventOptions::ManualReset), "native dispatch completion event");
        HRESULT result         = E_FAIL;
        std::size_t requests   = 0u;
        const auto publication = editor->GetAcknowledgedSemanticDocument(host);
        {
            std::jthread client([&]
            {
                // Only this caller's C++ allocations fail; the owner processes
                // the production bounded dispatch on its own message queue.
                countAllocations = true;
                allocations = allocationBytes = 0u;
                result                        = RichNativeToggleFaultOrigin(toggle.get(), failure);
                countAllocations              = false;
                requests                      = allocations;
                SetEvent(completed.get());
            });
            const auto deadline = GetTickCount64() + 15000u;
            const HANDLE event  = completed.get();
            while (WaitForSingleObject(event, 0u) != WAIT_OBJECT_0 && GetTickCount64() < deadline)
            {
                const auto wait = MsgWaitForMultipleObjects(1u, &event, FALSE, 50u, QS_ALLINPUT);
                Check(wait != WAIT_FAILED, "native dispatch waits for client or owner messages");
                MSG message{};
                while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
            Check(WaitForSingleObject(event, 0u) == WAIT_OBJECT_0, "native task dispatch finishes before watchdog");
        }
        if (failure == std::numeric_limits<std::size_t>::max())
        {
            Hr(result, "native foreign-thread task dispatch succeeds");
            Check(editor->GetText() == L"- [x] task\r\n" && editor->Undo() && editor->GetText() == source && ! editor->CanUndo(),
                  "successful native dispatched Toggle commits exactly one source transaction");
        }
        else
            Check(result == E_OUTOFMEMORY && editor->GetText() == source && ! editor->CanUndo() && ! editor->CanRedo() &&
                      editor->GetAcknowledgedSemanticDocument(host) == publication,
                  "each foreign-thread dispatch allocation fails through HRESULT before source/history/publication mutation");
        Check(GetFocus() == previousFocus, "native task dispatch and failures never take desktop focus");
        return requests;
    };
    const auto requests = invoke(std::numeric_limits<std::size_t>::max());
    Check(requests > 0u && requests < 128u, "native semantic dispatch has a finite observed caller allocation schedule");
    if (fatalChild)
    {
        static_cast<void>(invoke(0u));
        Check(false, "native semantic dispatch child must not continue after an allocation exception");
    }
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-uia-toggle-allocation"), "native semantic toggle allocation fails in its owned child");
    invoke(std::numeric_limits<std::size_t>::max());
    std::cout << "rich semantic native dispatch measuredRequests=" << requests << " sourcePreserved=1 retry=1 focusUnchanged=1\n";
}
__declspec(noinline) static HRESULT RichUiaCollectionFaultOrigin(ITextRangeProvider* document, ITableProvider* headers, unsigned scenario)
{
    DxUi::Failure::Scope scope;
    SAFEARRAY* result   = nullptr;
    failAllocationAfter = 0u;
    const auto hr       = scenario == 0u ? document->GetChildren(&result) : headers->GetColumnHeaders(&result);
    failAllocationAfter = std::numeric_limits<std::size_t>::max();
    if (result)
        SafeArrayDestroy(result);
    return hr;
}
static void TestRichSemanticAccessibilityFailureAtomicity(GraphicsFixture& gpu, std::optional<unsigned> fatalScenario = std::nullopt)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "semantic UIA failure shared graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "semantic UIA failure owner");
    const std::wstring source = L"| A | B |\n| - | - |\n| one |\n\n- [ ] task\n";
    auto root                 = std::make_unique<RichTextEditor>(source);
    auto* editor              = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(root));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "semantic UIA failure preparation");
    auto site = std::make_shared<TestEmbeddedAccessibilitySite>();
    const EmbeddedAccessibilityPlacement placement{{0, 0, 480, 240}, true};
    Hr(view.AttachAccessibility(site, 0x5249434855494146ull, placement), "semantic UIA failure attachment");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    Hr(view.GetAccessibilityProvider(provider.put()), "semantic UIA failure root");
    site->root        = provider.get();
    const auto detach = wil::scope_exit([&]
    {
        site->root = nullptr;
        view.Detach();
    });
    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment, documentFragment, tableFragment;
    Hr(provider.query_to(rootFragment.put()), "semantic root navigation interface");
    Hr(rootFragment->Navigate(NavigateDirection_FirstChild, documentFragment.put()), "semantic failure document");
    Hr(documentFragment->Navigate(NavigateDirection_FirstChild, tableFragment.put()), "semantic failure table");
    wil::com_ptr_nothrow<ITextProvider> text;
    wil::com_ptr_nothrow<IRawElementProviderSimple> table;
    Hr(documentFragment.query_to(text.put()), "semantic failure TextPattern");
    Hr(tableFragment.query_to(table.put()), "semantic failure table element");
    wil::com_ptr_nothrow<ITextRangeProvider> document;
    Hr(text->get_DocumentRange(document.put()), "semantic failure source range");
    wil::com_ptr_nothrow<IUnknown> unknown;
    Hr(table->GetPatternProvider(UIA_TablePatternId, unknown.put()), "semantic failure table pattern");
    wil::com_ptr_nothrow<ITableProvider> headers;
    Hr(unknown.query_to(headers.put()), "semantic failure header interface");
    const auto publication = editor->GetAcknowledgedSemanticDocument(view.Controls());
    std::size_t total      = 0u;
    for (unsigned scenario = 0u; scenario < 2u; ++scenario)
    {
        const auto invoke = [&]
        {
            SAFEARRAY* result = nullptr;
            const auto hr     = scenario == 0u ? document->GetChildren(&result) : headers->GetColumnHeaders(&result);
            if (result)
                SafeArrayDestroy(result);
            return hr;
        };
        std::size_t requests = 0u;
        {
            const auto oldCounting = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=]
            {
                countAllocations = oldCounting;
                allocations      = oldCalls;
                allocationBytes  = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations              = true;
            Hr(invoke(), "measure fallible semantic UIA collection");
            countAllocations = false;
            requests         = allocations;
        }
        Check(requests > 0u && requests < 256u, "semantic UIA collection has an independently observed bounded allocation schedule");
        if (fatalScenario && fatalScenario.value() == scenario)
        {
            static_cast<void>(RichUiaCollectionFaultOrigin(document.get(), headers.get(), scenario));
            Check(false, "semantic UIA collection child must not continue after allocation failure");
        }
        if (! fatalScenario)
            Check(DxUiFailureTests::ExpectFatalChild(scenario == 0u ? L"embedded-uia-children-allocation" : L"embedded-uia-headers-allocation"),
                  "semantic UIA collection allocation fails in its owned scenario child");
        Check(editor->GetText() == source && ! editor->CanUndo() && editor->GetAcknowledgedSemanticDocument(view.Controls()) == publication,
              "semantic UIA collection preserves source and publication");
        total += requests;
    }
    Check(site->focusRequests == 0u, "semantic collection queries never acquire application keyboard focus");
    std::cout << "rich semantic accessibility measuredRequests=" << total << " scenarios=2 sourcePreserved=1 retry=1\n";
    if (! fatalScenario)
        TestRichSemanticNativeDispatchFailureAtomicity();
}
__declspec(noinline) static void RichInputCacheFaultOrigin(DxUi::ControlHost& host, DxUi::RichTextEditor* editor, DxUi::NativeTextInputState& output)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    static_cast<void>(host.TryReadNativeTextInputState(editor, output));
    failAllocationAfter = std::numeric_limits<std::size_t>::max();
}
__declspec(noinline) static void RichCommandApplyFaultOrigin(DxUi::RichTextEditor& editor, const DxUi::RichText::MarkdownCommandPlan& plan)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    static_cast<void>(editor.ApplyMarkdownCommand(plan));
    failAllocationAfter = std::numeric_limits<std::size_t>::max();
}
static void TestRichMarkdownCommandHostFailureAtomicity(GraphicsFixture& gpu, unsigned fatalScenario = 0u)
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
    if (fatalScenario == 1u)
    {
        RichInputCacheFaultOrigin(host, editor, unavailable);
        Check(false, "native text cache child must not continue after allocation failure");
    }
    if (! fatalScenario)
        Check(DxUiFailureTests::ExpectFatalChild(L"embedded-text-cache-allocation"), "native text cache allocation fails in its owned child");
    Check(host.TryReadNativeTextInputState(editor, available) && available.text == source, "native text cache remains available");
    MarkdownCommandRequest request;
    request.command        = MarkdownCommand::ToggleTask;
    const auto original    = editor->Snapshot();
    const auto revision    = editor->Revision();
    const auto selection   = editor->GetSelection();
    const auto plan        = BuildMarkdownCommand(original, revision, editor->GetMarkdownSyntax(), selection, request);
    const auto composition = host.GetNativeTextCompositionActive(editor);
    if (fatalScenario == 2u)
    {
        RichCommandApplyFaultOrigin(*editor, plan);
        Check(false, "command application child must not continue after allocation failure");
    }
    if (! fatalScenario)
        Check(DxUiFailureTests::ExpectFatalChild(L"embedded-command-apply-allocation"), "command application allocation fails in its owned child");
    Check(composition.has_value() && ! composition.value(), "composition guard remains available");
    Check(editor->Snapshot().IsSameRevision(original) && editor->Revision() == revision && editor->GetSelection() == selection && ! editor->CanUndo(),
          "structural staging preserves source and selection before commit");
    Check(editor->ApplyMarkdownCommand(plan) == MarkdownCommandStatus::Changed && editor->Undo() && editor->Snapshot().IsSameRevision(original),
          "staging failure can retry once and undo exactly");
    std::cout << "rich command host cache failure: compositionAvailable=1 sourcePreserved=1 retryUndo=1\n";
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
        Document document(spelling);
        seed(document);
        const auto syntax   = RichCommandTests::Parse(document);
        const auto snapshot = document.Snapshot();
        const auto revision = document.Revision();
        const auto undo = document.CanUndo(), redo = document.CanRedo();
        std::optional<MarkdownCommandPlan> output;
        auto invalid    = request;
        invalid.command = static_cast<MarkdownCommand>(999u);
        Check(TryBuildMarkdownCommand(snapshot, revision, syntax, selection, invalid, output) == SourceResult::InvalidSource && ! output &&
                  document.Snapshot().IsSameRevision(snapshot) && document.Revision() == revision && document.GetSelection() == selection &&
                  document.CanUndo() == undo && document.CanRedo() == redo && syntax->Source() == spelling,
              "invalid structural request preserves source, selection, undo/redo and analysis");
        std::cout << "rich command allocation scenario=" << scenario << " measuredRequests=" << requests << '\n';
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
        bool changed       = true;
        const auto refused = TryApplySourceEdits(document, snapshot, revision + 1u, edits, {9u, 2u}, changed);
        Check(refused == SourceResult::Stale && ! changed && map->Source() == source && map->Display() == display &&
                  document.Snapshot().IsSameRevision(snapshot) && document.Revision() == revision && document.GetSelection() == selection &&
                  document.CanUndo() == undo && document.CanRedo() == redo,
              "stale semantic transaction preserves mapping and complete edit state");
        if (operation == 3u)
        {
            Check(ApplySourceEdits(document, snapshot, revision, edits, {9u, 2u}), "explicit transaction retry succeeds");
            Check(document.Undo() && document.Snapshot().IsSameRevision(snapshot), "transaction retry is one exact undo");
        }
        swept += requests;
        std::cout << "rich semantic allocation operation=" << operation << " measuredRequests=" << requests << '\n';
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
    std::cout << "rich semantic map measuredRequests=" << swept << " coordinateQueryAllocations=" << queryRequests << " maximumSourceUnits=" << maximum.size()
              << " maximumDisplayUnits=" << doubled.size() << " spans=" << maximumSpans.size() << " retainedCppBytes=" << retained
              << " releasedDelta=" << released << '\n';
}
