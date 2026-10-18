#pragma once

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
    DxUi::TextRangeUnitMoveResult forward, backward;
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
    Hr(view.Prepare(480, 240), "prepare rich allocation source");
    view.Controls().SetFocusControl(editor, false);
    Hr(view.Prepare(480, 240), "prepare focus before publishing rich allocation semantics");
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
        Hr(view.Prepare(480, 240), "prepare small/maximum rich source outside range-query budget");
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
    Hr(first.Prepare(480, 240), "rich-text first preparation");
    Hr(second.Prepare(320, 160, 144), "rich-text independent density preparation");
    Check(first.Controls().GetHwnd() == nullptr && second.Controls().GetHwnd() == nullptr, "retained rich text creates no embedded HWND");
    const auto prepared        = editor->DebugGetState();
    const auto otherGeneration = other->DebugGetState().layoutGeneration;
    Check(prepared.layoutReady && other->DebugGetState().layoutReady, "both layouts prepared through host DWrite");
    Check(first.GetStatistics().surfaceBytes != second.GetStatistics().surfaceBytes, "two views retain separate bounded surfaces");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "rich-text initial composition");
    std::vector<uint8_t> before;
    Hr(gpu.Read(before), "rich-text initial pixels");
    Check(first.Prepare(480, 240) == S_FALSE, "clean rich text does no preparation");
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
    Hr(first.Prepare(480, 240), "hidden concrete rich-text root preparation");
    Check(! editor->DebugGetState().layoutReady, "hidden root painting does not recreate retired layout");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "hidden rich-text root composition");
    std::vector<uint8_t> hidden;
    Hr(gpu.Read(hidden), "hidden rich-text root pixels");
    Check(hidden != before, "hidden root does not expose its text");
    editor->SetVisible(true);
    Hr(first.Prepare(480, 240), "shown concrete rich-text root preparation");
    gpu.Bind();
    Hr(first.Composite(gpu.context.get(), gpu.Viewport()), "shown rich-text root composition");
    std::vector<uint8_t> restored;
    Hr(gpu.Read(restored), "shown rich-text root pixels");
    Check(restored == before && first.GetStatistics().surfaceAllocations == statistics.surfaceAllocations,
          "show restores identical rich pixels without replacing the view surface");
    first.Controls().SetFocusControl(editor, false);
    editor->SetSelection({source.size(), source.size()});
    Hr(first.Prepare(480, 240), "rich-text logical focus");
    const auto revision = editor->Revision();
    Check(first.DispatchCharacter(wchar_t(0xd83d)) && editor->Revision() == revision, "pending embedded scalar remains unpublished");
    Check(first.DispatchCharacter(wchar_t(0xde00)) && editor->Revision() == revision + 1, "embedded scalar publishes one revision");
    Check(other->GetText() == source && other->DebugGetState().layoutGeneration == otherGeneration, "editing one view leaves the other independent");
    Check(requests > 0 && first.NeedsPreparation(), "editing schedules host preparation");
    Hr(first.Prepare(480, 240), "rich-text changed preparation");
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
    Hr(first.Prepare(480, 240), "rich-text after capture cancellation");
    Check(first.DispatchPointer({PointerAction::Down, 12, 16}) && first.DispatchPointer({PointerAction::Up, 12, 16}), "ordinary pointer release ends drag");
    Check(first.Controls().GetCapturedControl() == nullptr && ! editor->DebugGetState().dragging, "mouse up releases host capture");
    Check(editor->OnMouseDoubleClick(first.Controls(), D2D1::Point2F(12, 16), false, 0) && editor->GetSelection().Start() != editor->GetSelection().End(),
          "double click selects a displayed word using source offsets");
    Hr(first.Prepare(480, 240), "rich-text word selection");
    // Move outside double-click slop: this scenario begins a new drag.
    Check(first.DispatchPointer({PointerAction::Down, 200, 80}), "selection before Escape");
    Check(first.DispatchKey(VK_ESCAPE, true) && first.Controls().GetCapturedControl() == nullptr, "Escape cancels selection capture");
    editor->SetEnabled(false);
    Hr(first.Prepare(480, 240), "disabled rich-text preparation");
    Check(! first.DispatchCharacter(L'x'), "disabled rich-text receives no character input");
    editor->SetEnabled(true);
    editor->SetSelection({0, 0});
    Hr(first.Prepare(480, 240, 192), "rich-text DPI update");
    Check(editor->DebugGetState().layoutReady && editor->GetText().ends_with(L"\U0001f600"), "DPI update preserves source");
    const auto retained          = editor->Snapshot();
    const auto retainedSelection = editor->GetSelection();
    const bool canUndo           = editor->CanUndo();
    first.SetVisible(false);
    Check(first.GetStatistics().surfaceBytes == 0 && ! editor->DebugGetState().layoutReady && ! first.NeedsPreparation() && ! first.NeedsAnimation(),
          "hidden view releases pixels and layout without periodic work");
    Check(! first.DispatchCharacter(L'x') && first.Prepare(480, 240) == S_FALSE, "hidden view rejects input and preparation");
    first.SetVisible(true);
    Hr(first.Prepare(480, 240, 192), "rich-text show prepares again");
    Check(editor->GetText() == retained.Text(), "show preserves model");
    Hr(first.Prepare(0, 0), "zero-size rich text suspends");
    Check(first.GetStatistics().surfaceBytes == 0 && ! first.DispatchCharacter(L'x'), "zero size releases surface and blocks input");
    Hr(first.Prepare(480, 240), "restore rich-text extent");
    GraphicsFixture replacement;
    Hr(replacement.Create(), "rich-text replacement WARP device");
    std::shared_ptr<GraphicsDevice> nextGraphics;
    Hr(GraphicsDevice::Create(replacement.device.get(), nextGraphics), "rich-text replacement host pool");
    Hr(first.ReplaceDevice(nextGraphics), "rich-text device generation replacement");
    Check(! first.DispatchCharacter(L'x'), "replacement rejects input before coherent preparation");
    Hr(first.Prepare(480, 240), "rich-text replacement preparation");
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
    Hr(view.Prepare(480, 240), "retiring rich-text preparation");
    callback.retire = true;
    Check(editor->ReplaceSelection(L"committed before retirement"), "host scheduling can retire editing owner");
    Check(view.Controls().GetRoot() == nullptr && notices == 0, "retired callback batch stops before text notification");
    Hr(view.Prepare(480, 240), "prepare replacement empty root after retirement");
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
    Check(FAILED(view.Prepare(480, 240)), "rich-text raster exception fails preparation");
    gpu.Bind();
    Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_FALSE && ! view.DispatchCharacter(L'x'),
          "failed rich-text frame publishes neither pixels nor input");
    editor->fail = false;
    Hr(view.Prepare(480, 240), "explicit rich-text preparation retry");
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "recovered rich-text surface composes");
    Check(editor->GetText() == source.Text(), "failed preparation never changes logical document");
}
