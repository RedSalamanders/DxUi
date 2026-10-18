#pragma once

static void TestRichMarkdownParserFailureAndStorageBudgets()
{
    using namespace DxUi::RichText;
    const std::wstring source = L"# staged\r\n" + std::wstring(512u, L'\n') + L"&NotEqualTilde; &#x1f600;";
    const auto finish         = [](MarkdownParser& parser)
    {
        while (parser.HasPendingPreparation())
            parser.Advance(17u);
    };
    size_t requests = 0u;
    {
        MarkdownParser parser;
        parser.Begin(L"previous", 1u);
        finish(parser);
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
        parser.Begin(source, 2u);
        finish(parser);
        countAllocations = false;
        requests         = allocations;
    }
    Check(requests > 0u && requests < 128u, "lossless parser has a finite observed allocation schedule");
    for (size_t failure = 0u; failure < requests; ++failure)
    {
        MarkdownParser parser;
        parser.Begin(L"previous", 1u);
        finish(parser);
        const auto previous = parser.Published();
        bool rejected       = false;
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = failure;
            try
            {
                parser.Begin(source, 2u);
                finish(parser);
            }
            catch (const std::bad_alloc&)
            {
                rejected = true;
            }
        }
        Check(rejected && parser.Published() == previous && previous->Source() == L"previous" && ! parser.HasPendingPreparation(),
              "failure at every Begin/Advance allocation preserves published source and retires staged work");
        parser.Begin(source, 2u);
        finish(parser);
        Check(parser.Published()->Source() == source, "explicit retry after allocation failure succeeds");
    }
    const std::wstring maximum(Document::MaximumUnits, L'\n');
    ptrdiff_t retained = 0, released = 0;
    {
        const auto oldCounting = countLiveBytes;
        const auto oldLive     = liveBytes;
        const auto restore     = wil::scope_exit([=] noexcept
        {
            countLiveBytes = oldCounting;
            liveBytes      = oldLive;
        });
        liveBytes              = 0;
        countLiveBytes         = true;
        {
            MarkdownParser parser;
            parser.Begin(maximum, 1u);
            finish(parser);
            Check(parser.Published()->Blocks().size() == maximum.size() && parser.Published()->Lexemes().size() == maximum.size(),
                  "maximum physical-line fixture publishes all partitions without truncation");
            retained = liveBytes;
        }
        released       = liveBytes;
        countLiveBytes = false;
    }
    Check(retained >= 0 && retained <= 24 * 1024 * 1024 && released == 0, "maximum bounded syntax storage releases completely within 24 MiB C++ budget");
    MarkdownParser parser;
    parser.Begin(source, 1u);
    finish(parser);
    const auto oldCounting = countAllocations;
    const auto oldCalls = allocations, oldBytes = allocationBytes;
    size_t idleCalls = 0u;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        for (size_t repeat = 0u; repeat < 100u; ++repeat)
        {
            parser.Begin(source, 1u);
            static_cast<void>(parser.Published());
            parser.Advance(0u);
            parser.Cancel();
        }
        parser.Clear();
        countAllocations = false;
        idleCalls        = allocations;
    }
    Check(idleCalls == 0u, "same-content preparation, completed reads and retirement allocate no C++ storage");
    std::cout << "lossless Markdown sweptRequests=" << requests << " maximumRetainedCppBytes=" << retained << " releasedDelta=" << released
              << " unchangedAndRetireAllocations=" << idleCalls << '\n';
}

static void TestEmbeddedLosslessMarkdownPreparation(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "Markdown shared WARP pool");
    EmbeddedHost view, other;
    size_t requests = 0u;
    Hr(view.Attach(graphics, {&requests, [](void* context) noexcept { ++*static_cast<size_t*>(context); }}), "Markdown embedded view");
    Hr(other.Attach(graphics), "Markdown independent embedded view");
    const std::wstring source = L"# Unicode \U0001f600\r\n\n~~~lang\r\n# kept &NotEqualTilde;\n~~~\n[^unknown]: $x$\n";
    auto owned                = std::make_unique<RichTextEditor>(source);
    auto* editor              = owned.get();
    view.Controls().SetRoot(std::move(owned));
    view.Controls().SetFocusControl(editor, false);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare unchanged v0 pixels before optional analysis");
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose unchanged v0 pixels before analysis");
    std::vector<uint8_t> originalPixels;
    Hr(gpu.Read(originalPixels), "read unchanged v0 pixel oracle");
    auto independent        = std::make_unique<RichTextEditor>(source);
    auto* independentEditor = independent.get();
    independentEditor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    other.Controls().SetRoot(std::move(independent));
    Hr(PrepareRichEmbeddedContent(other, 320u, 160u, 144.0f), "prepare independent Markdown view");
    const auto independentSyntax = independentEditor->GetMarkdownSyntax();
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        editor->SetMarkdownAnalysisProfile(profile);
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare embedded lossless Markdown");
        const auto syntax = editor->GetMarkdownSyntax();
        Check(syntax && syntax->Source() == source && syntax->Profile() == profile && syntax->Blocks()[2].kind == MarkdownBlockKind::FencedCode &&
                  independentEditor->GetMarkdownSyntax() == independentSyntax && view.Controls().GetHwnd() == nullptr,
              "each supplied-device view owns independent keyed syntax without a renderer HWND");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose analysis-enabled unchanged preview");
        std::vector<uint8_t> pixels;
        Hr(gpu.Read(pixels), "read analysis-enabled preview pixels");
        Check(pixels == originalPixels, "lossless analysis does not silently replace or change the v0 formatted renderer");
    }
    const auto held = editor->GetMarkdownSyntax();
    editor->SetText(std::wstring(32000u, L'&'));
    Check(view.Prepare(480u, 240u) == S_FALSE && editor->DebugGetState().markdownPreparationPending && ! editor->GetMarkdownSyntax(),
          "embedded parser yields before geometry and never presents stale syntax as current");
    const auto advances    = editor->DebugGetState().markdownPreparationAdvances;
    const auto outstanding = requests;
    EmbeddedTextInputSnapshot input;
    Hr(view.ReadTextInput(input), "pending Markdown source text store");
    Check(input.state.text == editor->GetText() && ! input.caretBoundsDip, "parser pending source ACPs and unavailable geometry remain coherent");
    for (size_t key = 0u; key < 16u; ++key)
        Check(view.DispatchCharacter(L'\u03b2'), "pending Markdown Unicode edit");
    Check(requests == outstanding && editor->DebugGetState().markdownPreparationAdvances == advances && held->Source() == source,
          "input coalesces callbacks and never advances cancelled parse work or mutates held syntax");
    editor->SetText(source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "publish latest embedded Markdown");
    auto syntax = editor->GetMarkdownSyntax();
    Hr(PrepareRichEmbeddedContent(view, 360u, 180u, 144.0f), "reshape embedded Markdown without reparsing pure source");
    Check(editor->GetMarkdownSyntax() == syntax, "width and DPI do not invalidate completed source-only analysis");
    editor->SetText(std::wstring(32000u, L'x'));
    Check(view.Prepare(480u, 240u) == S_FALSE && editor->DebugGetState().markdownPreparationPending, "pending parser visibility fixture");
    view.SetVisible(false);
    const auto hiddenRequests = requests;
    Check(! editor->DebugGetState().markdownPreparationPending && view.Prepare(480u, 240u) == S_FALSE && requests == hiddenRequests,
          "hidden embedded view cancels staged parsing without periodic work");
    view.SetVisible(true);
    Check(view.Prepare(480u, 240u) == S_FALSE && editor->DebugGetState().markdownPreparationPending, "parser resumes after showing");
    Check(view.Prepare(480u, 0u) == S_FALSE && ! editor->DebugGetState().markdownPreparationPending, "zero-height extent cancels parser staging");
    Check(view.Prepare(480u, 240u) == S_FALSE, "parser resumes after positive extent");
    Hr(view.ReplaceDevice(graphics), "retire pending parser on embedded resource replacement");
    Check(! editor->DebugGetState().markdownPreparationPending, "resource replacement retires the pending owner job");
    editor->SetText(source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "resume visible source-only analysis");
    syntax = editor->GetMarkdownSyntax();
    editor->SetSelection({2u, 2u});
    Check(editor->ReplaceSelection(L"x") && editor->Undo() && editor->GetText() == source, "embedded analysis preserves source history");
    editor->SetMarkdownAnalysisProfile(std::nullopt);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "disable optional parser");
    Check(! editor->GetMarkdownSyntax() && editor->CanRedo() && syntax->Source() == source, "opt-out preserves held source and redo");
    view.Detach();
    Check(held->Source() == source && independentEditor->GetMarkdownSyntax() == independentSyntax,
          "held syntax survives detach without affecting another view");
}

static void TestRichTextCooperativeSchedulingAndTextInput(GraphicsFixture& gpu)
{
    using namespace DxUi;
    struct Requests
    {
        EmbeddedHost* view{};
        size_t count{};
        bool replaceRoot{};
        bool tryReenter{};
        HRESULT reentrantResult = S_OK;
    } requests;
    EmbeddedHost view, other;
    requests.view = &view;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "cooperative shared graphics");
    Hr(view.Attach(graphics,
                   {&requests,
                    [](void* context) noexcept
    {
        auto& request = *static_cast<Requests*>(context);
        ++request.count;
        if (std::exchange(request.tryReenter, false))
            request.reentrantResult = request.view->Prepare(480, 240);
        if (std::exchange(request.replaceRoot, false))
        {
            try
            {
                request.view->Controls().SetRoot(std::make_unique<Label>(L"replacement"));
            }
            catch (const std::exception&)
            {
                Check(false, "replacement scheduling callback completes");
            }
        }
    }}),
       "cooperative embedded host");
    Hr(other.Attach(graphics), "cooperative independent view");
    auto independent  = std::make_unique<RichTextEditor>(L"independent **source**");
    auto* otherEditor = independent.get();
    other.Controls().SetRoot(std::move(independent));
    Hr(PrepareRichEmbeddedContent(other, 320, 160, 144), "prepare independent density");
    const auto independentGeneration = otherEditor->DebugGetState().layoutGeneration;
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line)
        seed += L"line **bold** a\u0301 \U0001f600 text\r\n";
    auto owned   = std::make_unique<RichTextEditor>(seed);
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    view.Controls().SetFocusControl(editor, false);
    const auto beforeTurn = requests.count;
    requests.tryReenter   = true;
    Check(view.Prepare(480, 240) == S_FALSE && view.NeedsPreparation() && requests.count == beforeTurn + 1u,
          "one bounded embedded turn queues one continuation while pixels remain dirty");
    Check(requests.reentrantResult == E_UNEXPECTED, "a continuation callback cannot recursively drain preparation");
    const auto pending = editor->DebugGetState();
    Check(pending.preparationPending && pending.lastAdvanceLayouts <= 4u && pending.lastAdvanceBlockSteps <= 4096u,
          "embedded preparation obeys layout and source-block budgets");
    gpu.Bind();
    Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE && ! editor->TryGetTextInputCaretRect(view.Controls(), 0u) &&
              ! editor->GetVisibleSourceRange(view.Controls()),
          "pending pixels and geometry are unavailable");
    EmbeddedTextInputSnapshot stale;
    Hr(view.ReadTextInput(stale), "read exact source while embedded geometry is pending");
    Check(stale.state.text == seed && ! stale.caretBoundsDip, "pending source snapshot has no fabricated caret bounds");
    const auto outstanding = requests.count;
    for (size_t key = 0u; key < 48u; ++key)
        Check(view.DispatchCharacter(L'x'), "pending embedded owner accepts rapid Unicode source input");
    const auto latest = editor->GetText();
    Check(requests.count == outstanding && editor->DebugGetState().preparationAdvances == pending.preparationAdvances,
          "typing coalesces wakeups and never drains or resumes obsolete glyph work");
    Check(view.ApplyTextInput(stale.revision, stale.state, EmbeddedTextInputAction::Commit) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH) &&
              editor->GetText() == latest,
          "stale service result cannot overwrite newer pending source");
    EmbeddedTextInputSnapshot current;
    Hr(view.ReadTextInput(current), "read pending composition entry");
    auto preview = current.state;
    preview.text += L"\u03b2";
    preview.caretIndex = preview.text.size();
    preview.selectionAnchorIndex.reset();
    preview.compositionStartIndex = latest.size();
    preview.compositionEndIndex   = preview.text.size();
    Hr(view.ApplyTextInput(current.revision, preview, EmbeddedTextInputAction::Preview), "IME preview remains available during reflow");
    Hr(view.ReadTextInput(current), "read pending composition revision");
    Hr(view.ApplyTextInput(current.revision, {}, EmbeddedTextInputAction::Cancel), "cancel pending composition transaction");
    Check(editor->GetText() == latest && otherEditor->DebugGetState().layoutGeneration == independentGeneration,
          "composition cancellation preserves source/history and the independent view");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "publish latest coalesced embedded source");
    Hr(view.ReadTextInput(current), "read current prepared source");
    Check(current.state.text == latest && current.caretBoundsDip && ! view.NeedsPreparation(), "only final source/geometry becomes coherent");
    editor->SetText(seed);
    view.Controls().CaptureMouse(editor); // Embedded logical capture owns no OS capture.
    Check(view.Controls().GetCapturedControl() == editor, "pending source fixture begins with logical capture");
    Check(view.Prepare(480, 240) == S_FALSE && editor->DebugGetState().preparationPending, "enter pending zero-size fixture");
    Check(view.Controls().GetCapturedControl() == nullptr, "pending geometry cancels capture before an unavailable Up can be lost");
    const auto zeroRequests = requests.count;
    Check(view.Prepare(0, 0) == S_FALSE && ! editor->DebugGetState().preparationPending && view.GetStatistics().surfaceBytes == 0u &&
              requests.count == zeroRequests,
          "zero extent cancels staging and adds no periodic work");
    Check(view.Prepare(480, 240) == S_FALSE, "resumed extent starts fresh bounded work");
    const auto oldResources = view.Controls().GetResourceGeneration();
    Hr(view.ReplaceDevice(graphics), "replace resources with the same graphics pointer");
    Check(view.Controls().GetResourceGeneration() != oldResources && ! editor->DebugGetState().preparationPending &&
              editor->DebugGetState().retainedGeometryChunks == 0u && editor->GetText() == seed,
          "resource epoch cancels old work even when device/pool addresses repeat");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare after resource replacement");
    editor->SetText(seed + L"x");
    Check(view.Prepare(480, 240) == S_FALSE, "enter pending density and extent fixture");
    const auto cancellations = editor->DebugGetState().cancelledPreparations;
    Check(view.Prepare(360, 180, 144) == S_FALSE && editor->DebugGetState().cancelledPreparations > cancellations && editor->GetText() == seed + L"x",
          "changed DPI and width replace pending work without publishing the old key");
    editor->SetDensity(Density::Compact);
    Check(! editor->DebugGetState().preparationPending, "embedded density change retires the pending viewport request");
    Hr(PrepareRichEmbeddedContent(view, 360, 180, 144), "publish only the new embedded typography and width");
    editor->SetText(seed);
    Check(view.Prepare(480, 240) == S_FALSE, "enter pending hidden fixture");
    view.SetVisible(false);
    const auto hiddenRequests = requests.count;
    Check(! editor->DebugGetState().preparationPending && view.Prepare(480, 240) == S_FALSE && requests.count == hiddenRequests,
          "hidden view retires preparation without wakeups");
    view.SetVisible(true);
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "resume visible cooperative view");
    editor->SetText(seed);
    const auto lifetime  = GetControlLifetimeToken(*editor);
    requests.replaceRoot = true;
    Check(view.Prepare(480, 240) == S_FALSE && lifetime.expired(), "continuation callback may retire its editor safely");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare replacement root after callback retirement");
    Check(dynamic_cast<Label*>(view.Controls().GetRoot()) != nullptr && ! view.NeedsPreparation(), "retired editor work never replaces the current tree");
}

static void TestRichTextCooperativeFailureDoesNotRetryAutomatically(GraphicsFixture& gpu)
{
    using namespace DxUi;
    class FailingPreparation final : public RichTextEditor
    {
    public:
        FailingPreparation() : RichTextEditor(L"source survives")
        {
        }
        bool fail = true;
        HRESULT PrepareContent(ControlHost& host) noexcept override
        {
            return fail ? E_OUTOFMEMORY : RichTextEditor::PrepareContent(host);
        }
    };
    EmbeddedHost view;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "failing cooperative graphics");
    size_t requests = 0u;
    Hr(view.Attach(graphics, {&requests, [](void* context) noexcept { ++*static_cast<size_t*>(context); }}), "failing cooperative host");
    auto owned   = std::make_unique<FailingPreparation>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    const auto beforeFailure = requests;
    Check(FAILED(view.Prepare(480, 240)) && view.NeedsPreparation() && requests == beforeFailure,
          "preparation failure suppresses publication and schedules no automatic retry");
    editor->fail = false;
    editor->SetText(L"new source permits retry");
    Check(requests == beforeFailure + 1u, "a new model request wakes a failed dirty view exactly once");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "retry new source after explicit change");
    Check(editor->GetText() == L"new source permits retry" && editor->DebugGetState().layoutReady, "explicit retry publishes retained source");
}

// Included by the supplied-device fixture after its Check/Hr and allocation counter.
static void TestUnicodeGraphemeForwardWalkAllocationBudget()
{
    // Four independent elements per pattern: Hangul, Indic, pictographic ZWJ,
    // ASCII. The caller prepares the source before the measured forward walk.
    std::wstring source;
    constexpr size_t repetitions = 4096u;
    for (size_t index = 0; index < repetitions; ++index)
        source += L"\u1100\u1161\u11a8\u0915\u094d\u0915\U0001f469\u200d\U0001f4bbx";
    const bool previousCounting    = countAllocations;
    const auto previousAllocations = allocations, previousBytes = allocationBytes;
    size_t elements = 0u, cursor = 0u, calls = 0u, bytes = 0u;
    bool progress = true;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = previousCounting;
            allocations      = previousAllocations;
            allocationBytes  = previousBytes;
        });
        allocationBytes = allocations = 0u;
        countAllocations              = true;
        while (cursor < source.size() && elements <= source.size())
        {
            const auto next = DxUi::FindNextTextElementBoundary(source, cursor);
            if (next <= cursor || next > source.size())
            {
                progress = false;
                break;
            }
            cursor = next;
            ++elements;
        }
        countAllocations = false;
        calls            = allocations;
        bytes            = allocationBytes;
    }
    std::cout << "Unicode17 forward walk units=" << source.size() << " elements=" << elements << " CppAllocations=" << calls << " CppRequestedBytes=" << bytes
              << '\n';
    Check(progress && cursor == source.size() && elements == repetitions * 4u,
          "forward Unicode traversal makes bounded progress through independent mixed-script elements");
    Check(calls == 0u && bytes == 0u, "forward grapheme traversal owns no C++ heap storage");
}

static void TestUnicodeBulkMovementAndDeletionAllocationBudget()
{
    std::wstring source;
    std::vector<size_t> boundaries{0u};
    constexpr int repetitions = 4096, expectedElements = repetitions * 4;
    for (int index = 0; index < repetitions; ++index)
    {
        const auto first = source.size();
        source += L"\u1100\u1161\u11a8\u0915\u094d\u0915\U0001f469\u200d\U0001f4bbx";
        for (const auto end : {3u, 6u, 11u, 12u})
            boundaries.push_back(first + end);
    }
    const bool previousCounting    = countAllocations;
    const auto previousAllocations = allocations, previousBytes = allocationBytes;
    DxUi::TextRangeUnitMoveResult forward, backward, singleBackward;
    std::array<DxUi::TextElementRange, 4> deletions;
    size_t calls = 0u, bytes = 0u;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = previousCounting;
            allocations      = previousAllocations;
            allocationBytes  = previousBytes;
        });
        allocationBytes = allocations = 0u;
        countAllocations              = true;
        forward                       = DxUi::MoveAccessibilityTextPositionByUnit(source, 0u, TextUnit_Character, INT_MAX);
        backward                      = DxUi::MoveAccessibilityTextPositionByUnit(source, source.size(), TextUnit_Character, INT_MIN);
        singleBackward                = DxUi::MoveAccessibilityTextPositionByUnit(source, source.size(), TextUnit_Character, -1);
        deletions[0]                  = DxUi::GetTextElementDeletionRange(source, 1u, false);
        deletions[1]                  = DxUi::GetTextElementDeletionRange(source, 1u, true);
        deletions[2]                  = DxUi::GetTextElementDeletionRange(source, 1u, false, boundaries);
        deletions[3]                  = DxUi::GetTextElementDeletionRange(source, 1u, true, boundaries);
        countAllocations              = false;
        calls                         = allocations;
        bytes                         = allocationBytes;
    }
    std::cout << "Unicode17 bulk commands units=" << source.size() << " elements=" << expectedElements << " CppAllocations=" << calls
              << " CppRequestedBytes=" << bytes << '\n';
    Check(forward.position == source.size() && forward.moved == expectedElements && backward.position == 0u && backward.moved == -expectedElements,
          "extreme bulk commands return independent endpoints and exact signed counts");
    for (const auto range : deletions)
        Check(range.first == 0u && range.last == 3u, "indexed and streaming deletion enclose the complete first Hangul element");
    Check(singleBackward.position == source.size() - 1u && singleBackward.moved == -1, "single backward command keeps the independent final ASCII boundary");
    Check(calls == 0u && bytes == 0u, "bulk character movement and collapsed deletion request no C++ heap storage");
}

static void TestRichTextUtf8RejectionAllocationBudget()
{
    for (const auto inputBytes : {DxUi::RichText::Document::MaximumUnits + 1u, DxUi::RichText::Document::MaximumUnits * 3u + 1u})
    {
        const std::string input(inputBytes, 'a'); // Caller-owned input is outside the decoder's allocation budget.
        const bool previousCounting    = countAllocations;
        const auto previousAllocations = allocations, previousBytes = allocationBytes;
        bool lengthRejected   = false;
        size_t requestedBytes = 0u;
        {
            const auto restore = wil::scope_exit([=] noexcept
            {
                countAllocations = previousCounting;
                allocations      = previousAllocations;
                allocationBytes  = previousBytes;
            });
            allocationBytes = allocations = 0u;
            countAllocations              = true;
            try
            {
                static_cast<void>(DxUi::RichText::DecodeUtf8(input));
            }
            catch (const std::length_error&)
            {
                lengthRejected = true;
            }
            countAllocations = false;
            requestedBytes   = allocationBytes;
        }
        std::cout << "rich UTF8 rejection inputBytes=" << inputBytes << " decoderCppRequestedBytes=" << requestedBytes << '\n';
        Check(lengthRejected, "oversized UTF8 source returns a length error");
        Check(requestedBytes <= 4096u, "oversized UTF8 rejection never materializes the oversized UTF16 result");
    }
}

static void TestRetainedRichTextUiaRangeAllocationBudget(GraphicsFixture& gpu)
{
    using namespace DxUi;
    const bool previousCounting      = countAllocations;
    const size_t previousAllocations = allocations, previousBytes = allocationBytes;
    // This metadata probe must not contaminate the suite's separate absolute
    // warm-composition counter, which starts at zero and is checked later.
    const auto restoreCounters = wil::scope_exit([=] noexcept
    {
        countAllocations = previousCounting;
        allocations      = previousAllocations;
        allocationBytes  = previousBytes;
    });
    countAllocations           = false;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "rich UIA allocation pool");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "rich UIA allocation view");
    auto root    = std::make_unique<RichTextEditor>(L"initial");
    auto* editor = root.get();
    view.Controls().SetRoot(std::move(root));
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare rich allocation source");
    view.Controls().SetFocusControl(editor, false);
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare focus before publishing rich allocation semantics");
    auto site = std::make_shared<TestEmbeddedAccessibilitySite>();
    const EmbeddedAccessibilityPlacement placement{{0, 0, 480, 240}, true};
    Check(view.AttachAccessibility(site, 0x5249434842554447ull, placement) == S_OK, "attach coherent rich allocation semantics");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    Check(view.GetAccessibilityProvider(provider.put()) == S_OK && provider, "coherent rich allocation root is present");
    site->root        = provider.get();
    const auto detach = wil::scope_exit([&]
    {
        site->root = nullptr;
        view.Detach();
    });
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
    Hr(provider->ElementProviderFromPoint(16, 16, fragment.put()), "rich allocation document element");
    Check(bool(fragment), "rich source is reachable in the allocation fixture");
    wil::com_ptr_nothrow<ITextProvider2> text;
    Hr(fragment.query_to(text.put()), "rich allocation TextPattern2");
    for (const size_t units : {size_t(32), RichText::Document::MaximumUnits})
    {
        editor->SetText(std::wstring(units, L'a'));
        Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare small/maximum rich source outside range-query budget");
        Check(view.UpdateAccessibility(placement) == S_OK, "publish small/maximum rich source");
        wil::com_ptr_nothrow<ITextRangeProvider> document;
        Hr(text->get_DocumentRange(document.put()), "read published source outside metadata budget");
        Check(bool(document), "published source range exists");
        wil::unique_bstr source;
        Hr(document->GetText(-1, source.put()), "read actual published source length");
        Check(SysStringLen(source.get()) == units, "allocation fixture really publishes the requested small/maximum source");
        BOOL active = FALSE;
        wil::com_ptr_nothrow<ITextRangeProvider> warm;
        Hr(text->GetCaretRange(&active, warm.put()), "warm rich caret metadata");
        warm.reset();
        size_t largestRequest = 0;
        for (size_t iteration = 0; iteration < 8; ++iteration)
        {
            wil::com_ptr_nothrow<ITextRangeProvider> caret;
            SAFEARRAY* visible = nullptr;
            allocationBytes = allocations = 0;
            countAllocations              = true;
            const HRESULT caretResult     = text->GetCaretRange(&active, caret.put());
            const HRESULT visibleResult   = text->GetVisibleRanges(&visible);
            countAllocations              = false;
            largestRequest                = std::max(largestRequest, allocationBytes);
            const auto destroy            = wil::scope_exit([&]
            {
                if (visible)
                    SafeArrayDestroy(visible);
            });
            Hr(caretResult, "bounded caret metadata query");
            Hr(visibleResult, "bounded visible-range metadata query");
            Check(active && caret && visible, "queries retain current range identities");
        }
        // Range metadata may allocate provider objects. It must not duplicate
        // a 128 KiB maximum document for each caret/visible-range request.
        Check(largestRequest <= 4096, "rich range C++ allocation budget is independent of source size");
        std::cout << "rich UIA ranges sourceUnits=" << units << " largestMetadataRequestBytes=" << largestRequest << '\n';
    }
}

static void TestRetainedRichTextDefaultFontChunkReuse(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "positioned rich-text shared device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "positioned rich-text embedded view");
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line)
        seed += L"line **bold** a\u0301 \U0001f600 text\r\n";
    auto control = std::make_unique<RichTextEditor>(seed);
    auto* editor = control.get();
    view.Controls().SetRoot(std::move(control));
    view.Controls().SetFocusControl(editor, false);
    for (const float dpi : {96.0f, 144.0f, 192.0f})
    {
        Hr(PrepareRichEmbeddedContent(view, 480, 240, dpi), "positioned rich-text embedded initial preparation");
        editor->SetSelection({seed.size() / 2u, seed.size() / 2u + 1u});
        Check(editor->ReplaceSelection(dpi == 144.0f ? L"y" : L"x"), "positioned embedded fixture changes display text");
        Hr(PrepareRichEmbeddedContent(view, 480, 240, dpi), "positioned rich-text embedded edited preparation");
        const auto state = editor->DebugGetState();
        auto* format     = view.Controls().GetTextFormat(FontRole::Body);
        Check(format && format->GetFontSize() == 13.0f && state.layoutReady && state.retainedGeometryChunks > 1u && state.retainedGeometryChunks <= 128u &&
                  state.reusedGeometryChunks > 0u && state.preparedSourceUnits <= 2048u && ! state.wholeGeometryFallback && ! state.wholeRasterFallback &&
                  state.wholeLayoutsPrepared == 0u && state.wholeLayoutSourceUnits == 0u,
              "actual default 13-DIP embedded editor reuses positioned chunks at each DPI without hidden whole-layout work");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "positioned embedded cached composition");
        EmbeddedTextInputSnapshot snapshot;
        Hr(view.ReadTextInput(snapshot), "positioned embedded source text input snapshot");
        Check(snapshot.state.text == editor->GetText() && snapshot.state.text.size() == seed.size(),
              "private layout-origin units never enter the embedded text store");
        std::cout << "rich embedded positioned dpi=" << dpi << " bodyFontSize=" << format->GetFontSize() << " chunks=" << state.retainedGeometryChunks
                  << " reusedChunks=" << state.reusedGeometryChunks << " preparedSourceUnits=" << state.preparedSourceUnits << '\n';
    }
}
static void TestRetainedRichTextSharedViewsAndResources(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "rich-text shared WARP pool");
    EmbeddedHost first, second;
    size_t requests = 0;
    Hr(first.Attach(graphics, {&requests, [](void* context) noexcept { ++*static_cast<size_t*>(context); }}), "rich-text first view");
    Hr(second.Attach(graphics), "rich-text second view");
    const std::wstring source = L"# Owned Unicode\r\n**bold** \u03b2 \U0001f600 a\u0301\n\u05d0\u05d1\u05d2\nlast";
    auto a                    = std::make_unique<RichTextEditor>(source);
    auto b                    = std::make_unique<RichTextEditor>(source);
    auto* editor              = a.get();
    auto* other               = b.get();
    first.Controls().SetRoot(std::move(a));
    second.Controls().SetRoot(std::move(b));
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text first preparation");
    Hr(PrepareRichEmbeddedContent(second, 320, 160, 144), "rich-text independent density preparation");
    Check(first.Controls().GetHwnd() == nullptr && second.Controls().GetHwnd() == nullptr, "retained rich text creates no embedded HWND");
    const auto prepared        = editor->DebugGetState();
    const auto otherGeneration = other->DebugGetState().layoutGeneration;
    Check(prepared.layoutReady && other->DebugGetState().layoutReady, "both layouts prepared through host DWrite");
    Check(first.GetStatistics().surfaceBytes != second.GetStatistics().surfaceBytes, "two views retain separate bounded surfaces");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "rich-text initial composition");
    std::vector<uint8_t> before;
    Hr(gpu.Read(before), "rich-text initial pixels");
    Check(PrepareRichEmbeddedContent(first, 480, 240) == S_FALSE, "clean rich text does no preparation");
    const auto statistics          = first.GetStatistics();
    const size_t allocationsBefore = allocations;
    countAllocations               = true;
    for (int frame = 0; frame != 12; ++frame)
        Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "warm rich-text composition");
    countAllocations = false;
    Check(allocations == allocationsBefore, "warm rich-text composition allocates no C++ heap memory");
    Check(first.GetStatistics().preparations == statistics.preparations && first.GetStatistics().surfaceAllocations == statistics.surfaceAllocations &&
              editor->DebugGetState().layoutGeneration == prepared.layoutGeneration,
          "composition does no layout rasterization or surface allocation");

    // Control visibility differs from suspending the complete view. In both
    // cases the retired rich layout must stay retired until the control is shown.
    editor->SetVisible(false);
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "hidden concrete rich-text root preparation");
    Check(! editor->DebugGetState().layoutReady, "hidden root painting does not recreate retired layout");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "hidden rich-text root composition");
    std::vector<uint8_t> hidden;
    Hr(gpu.Read(hidden), "hidden rich-text root pixels");
    Check(hidden != before, "hidden root does not expose its text");
    editor->SetVisible(true);
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "shown concrete rich-text root preparation");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "shown rich-text root composition");
    std::vector<uint8_t> restored;
    Hr(gpu.Read(restored), "shown rich-text root pixels");
    Check(restored == before && first.GetStatistics().surfaceAllocations == statistics.surfaceAllocations,
          "show restores identical rich pixels without replacing the view surface");
    first.Controls().SetFocusControl(editor, false);
    editor->SetSelection({source.size(), source.size()});
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text logical focus");
    const auto revision = editor->Revision();
    Check(first.DispatchCharacter(wchar_t(0xd83d)) && editor->Revision() == revision, "pending embedded scalar remains unpublished");
    Check(first.DispatchCharacter(wchar_t(0xde00)) && editor->Revision() == revision + 1, "embedded scalar publishes one revision");
    Check(other->GetText() == source && other->DebugGetState().layoutGeneration == otherGeneration, "editing one view leaves the other independent");
    Check(requests > 0 && first.NeedsPreparation(), "editing schedules host preparation");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text changed preparation");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "rich-text changed composition");
    std::vector<uint8_t> after;
    Hr(gpu.Read(after), "rich-text changed pixels");
    Check(before != after, "retained source edit changes actual WARP pixels");
    EmbeddedTextInputSnapshot unsupported;
    Check(first.ReadTextInput(unsupported) == S_OK && unsupported.state.multiline, "D2d exposes source text through the shared embedded adapter");
    Check(first.DispatchPointer({PointerAction::Down, 12, 16}), "rich-text embedded pointer selection");
    Check(first.Controls().GetCapturedControl() == editor && editor->DebugGetState().dragging, "selection uses host capture");
    Check(first.DispatchPointer({PointerAction::Move, 110, 42}), "rich-text captured selection extends");
    Check(editor->GetSelection().Start() != editor->GetSelection().End(), "drag updates source selection");
    Check(first.DispatchPointer({PointerAction::Cancel}), "rich-text pointer cancellation");
    Check(first.Controls().GetCapturedControl() == nullptr && ! editor->DebugGetState().dragging, "capture loss retires drag");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text after capture cancellation");
    Check(first.DispatchPointer({PointerAction::Down, 12, 16}) && first.DispatchPointer({PointerAction::Up, 12, 16}), "ordinary pointer release ends drag");
    Check(first.Controls().GetCapturedControl() == nullptr && ! editor->DebugGetState().dragging, "mouse up releases host capture");
    Check(editor->OnMouseDoubleClick(first.Controls(), D2D1::Point2F(12, 16), false, 0) && editor->GetSelection().Start() != editor->GetSelection().End(),
          "double click selects a displayed word using source offsets");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text word selection");
    // Move outside double-click slop: this scenario begins a new drag.
    Check(first.DispatchPointer({PointerAction::Down, 200, 80}), "selection before Escape");
    Check(first.DispatchKey(VK_ESCAPE, true) && first.Controls().GetCapturedControl() == nullptr, "Escape cancels selection capture");
    editor->SetEnabled(false);
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "disabled rich-text preparation");
    Check(! first.DispatchCharacter(L'x'), "disabled rich-text receives no character input");
    editor->SetEnabled(true);
    editor->SetSelection({0, 0});
    Hr(PrepareRichEmbeddedContent(first, 480, 240, 192), "rich-text DPI update");
    Check(editor->DebugGetState().layoutReady && editor->GetText().ends_with(L"\U0001f600"), "DPI update preserves source");
    const auto retained          = editor->Snapshot();
    const auto retainedSelection = editor->GetSelection();
    const bool canUndo           = editor->CanUndo();
    first.SetVisible(false);
    Check(first.GetStatistics().surfaceBytes == 0 && ! editor->DebugGetState().layoutReady && ! first.NeedsPreparation() && ! first.NeedsAnimation(),
          "hidden view releases pixels and layout without periodic work");
    Check(! first.DispatchCharacter(L'x') && PrepareRichEmbeddedContent(first, 480, 240) == S_FALSE, "hidden view rejects input and preparation");
    first.SetVisible(true);
    Hr(PrepareRichEmbeddedContent(first, 480, 240, 192), "rich-text show prepares again");
    Check(editor->GetText() == retained.Text(), "show preserves model");
    Hr(PrepareRichEmbeddedContent(first, 0, 0), "zero-size rich text suspends");
    Check(first.GetStatistics().surfaceBytes == 0 && ! first.DispatchCharacter(L'x'), "zero size releases surface and blocks input");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "restore rich-text extent");
    GraphicsFixture replacement;
    Hr(replacement.Create(), "rich-text replacement WARP device");
    std::shared_ptr<GraphicsDevice> nextGraphics;
    Hr(GraphicsDevice::Create(replacement.device.get(), nextGraphics), "rich-text replacement host pool");
    Hr(first.ReplaceDevice(nextGraphics), "rich-text device generation replacement");
    Check(! first.DispatchCharacter(L'x'), "replacement rejects input before coherent preparation");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text replacement preparation");
    replacement.Bind();
    Hr(first.Composite(replacement.context.get(), replacement.Viewport()), "rich-text replacement composition");
    Check(editor->GetText() == retained.Text() && editor->CanUndo() == canUndo && editor->GetSelection() == retainedSelection,
          "device replacement preserves document selection and history");
    Check(editor->Undo() && editor->GetText() == source, "history remains usable after resource recovery");
}

static void TestRetainedRichTextSchedulingCanRetireOwner(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "retiring rich-text pool");
    struct CallbackState
    {
        EmbeddedHost* view = nullptr;
        bool retire        = false;
    } callback;
    EmbeddedHost view;
    callback.view = &view;
    Hr(view.Attach(graphics,
                   {&callback,
                    [](void* context) noexcept
    {
        auto& state = *static_cast<CallbackState*>(context);
        if (std::exchange(state.retire, false))
            state.view->Controls().SetRoot(nullptr);
    }}),
       "retiring rich-text view");
    auto control   = std::make_unique<RichTextEditor>();
    auto* editor   = control.get();
    size_t notices = 0;
    editor->SetOnTextChanged([&] { ++notices; });
    view.Controls().SetRoot(std::move(control));
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "retiring rich-text preparation");
    callback.retire = true;
    Check(editor->ReplaceSelection(L"committed before retirement"), "host scheduling can retire editing owner");
    Check(view.Controls().GetRoot() == nullptr && notices == 0, "retired callback batch stops before text notification");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare replacement empty root after retirement");
}

static void TestRetainedRichTextFailedPreparation(GraphicsFixture& gpu)
{
    class FailingEditor final : public DxUi::RichTextEditor
    {
    public:
        FailingEditor() : RichTextEditor(L"# Retained source\n**kept**")
        {
        }
        mutable bool fail = true;
        void Paint(DxUi::ControlHost& host) const override
        {
            RichTextEditor::Paint(host);
            if (fail)
                throw std::runtime_error("injected post-raster preparation failure");
        }
    };
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "failed rich-text preparation pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(graphics), "failed rich-text preparation host");
    auto owned   = std::make_unique<FailingEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    const auto source = editor->Snapshot();
    Check(FAILED(PrepareRichEmbeddedContent(view, 480, 240)), "rich-text raster exception fails preparation");
    gpu.Bind();
    Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE && ! view.DispatchCharacter(L'x'),
          "failed rich-text frame publishes neither pixels nor input");
    editor->fail = false;
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "explicit rich-text preparation retry");
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "recovered rich-text surface composes");
    Check(editor->GetText() == source.Text(), "failed preparation never changes logical document");
}
