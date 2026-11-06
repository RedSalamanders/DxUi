#pragma once
#include "Support.Tests.FlowchartLayout.h"
#include "Support.Tests.Footnotes.h"
#include "Support.Tests.MarkdownBlocks.h"
#include "Support.Tests.MarkdownCapacity.h"
#include "Support.Tests.MarkdownInlines.h"
#include "Support.Tests.PreparedFlowchart.h"
#include "Support.Tests.RichFileDrop.h"
#include "Support.Tests.RichImage.h"
#include "Support.Tests.RichImagePaths.h"
#include "Support.Tests.RichImageResources.h"
#include "Support.Tests.RichTextExport.h"
#include "Support.Tests.SemanticDiagrams.h"
#include "Support.Tests.SemanticSourceBlock.h"
#include <DxUi/RichSemanticDocument.h>
#include <DxUi/RichTextCommands.h>
#include <type_traits>

static void TestRichExportAllocationFailures()
{
    using namespace DxUi::RichText;
    for (const auto* name : {L"embedded-codec-allocation",
                             L"embedded-document-allocation",
                             L"embedded-projection-allocation",
                             L"embedded-command-allocation",
                             L"embedded-parser-allocation",
                             L"embedded-semantic-allocation",
                             L"embedded-export-allocation",
                             L"embedded-geometry-allocation"})
        Check(DxUiFailureTests::ExpectFatalChild(name), "owned rich allocation child reports a matching original-site dump");
    for (unsigned operation = 0u; operation < 4u; ++operation)
    {
        const auto convert = [operation]
        {
            if (operation == 0u)
                static_cast<void>(EncodeUtf8(L"heading"));
            else if (operation == 1u)
                static_cast<void>(DecodeUtf8("heading"));
            else if (operation == 2u)
                static_cast<void>(EncodeUtf8(L""));
            else
                static_cast<void>(DecodeUtf8(""));
        };
        size_t observed{};
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
            convert();
            observed = allocations;
        }
        Check(observed < 64u, "UTF-8 conversion allocation schedule is bounded");
    }
    const auto document =
        RichSemanticDocumentTests::Build(L"# Caf\u00e9\n\n1. **one**\n2. [two](https://example.test) [go](#caf%C3%A9)\n", MarkdownProfile::RichMarkdown);
    const std::wstring source(document->Syntax()->Source());
    const std::wstring display(document->Map().Display());
    size_t requests{};
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
        const auto exported           = ExportSemanticDocument(*document);
        requests                      = allocations;
        Check(! exported.Text().empty() && ! exported.HtmlFragment().empty(), "measure complete export allocation points");
    }
    Check(requests > 0u && requests < 512u, "export allocation sweep has a finite observed schedule");
    std::optional<ExportContent> output;
    Check(TryExportSemanticDocument(*document, {display.size(), display.size() + 1u}, output) == SourceResult::InvalidRange && ! output &&
              document->Syntax()->Source() == source && document->Map().Display() == display,
          "invalid export range preserves publication and does not publish partial output");
}

static void TestRichMetadataOnlyComposition(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "metadata-only shared device");
    for (const auto mode : {RichText::ViewMode::Rich, RichText::ViewMode::Source})
    {
        EmbeddedHost view;
        Hr(view.Attach(graphics), "metadata-only embedded owner");
        auto owned   = std::make_unique<RichTextEditor>(L"word");
        auto* editor = owned.get();
        editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
        editor->SetMarkdownRenderingEnabled(true);
        editor->SetViewMode(mode);
        editor->SetSelection({4u, 4u});
        view.Controls().SetRoot(std::move(owned));
        view.Controls().SetFocusControl(editor, false);
        const auto pixels = [&]
        {
            Hr(PrepareRichEmbeddedContent(view, gpu.width, gpu.height), "prepare metadata-only overlays");
            gpu.Bind();
            Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose metadata-only surface");
            std::vector<uint8_t> result;
            Hr(gpu.Read(result), "read metadata-only pixels");
            return result;
        };
        const auto before      = pixels();
        const auto source      = editor->Snapshot();
        const auto revision    = editor->Revision();
        const auto selection   = editor->GetSelection();
        const auto generation  = editor->DebugGetState().layoutGeneration;
        unsigned notifications = 0u;
        editor->SetOnTextChanged([&] { ++notifications; });
        editor->SetOnSelectionChanged([&] { ++notifications; });
        EmbeddedTextInputSnapshot snapshot;
        Hr(view.ReadTextInput(snapshot), "read metadata-only entry state");
        auto composing                        = snapshot.state;
        composing.compositionStartIndex       = 0u;
        composing.compositionEndIndex         = 4u;
        composing.compositionClauseBoundaries = {0u, 4u};
        composing.conversionTargetStartIndex  = 1u;
        composing.conversionTargetEndIndex    = 2u;
        Hr(view.ApplyTextInput(snapshot.revision, composing, EmbeddedTextInputAction::Preview), "publish metadata without text or selection changes");
        const auto marked = pixels();
        Check(marked != before, "metadata-only composition paints its underline over unchanged text");
        Hr(view.ReadTextInput(snapshot), "read conversion-only revision");
        composing                            = snapshot.state;
        composing.conversionTargetStartIndex = 2u;
        composing.conversionTargetEndIndex   = 3u;
        Hr(view.ApplyTextInput(snapshot.revision, composing, EmbeddedTextInputAction::Preview), "move only the conversion target");
        Check(pixels() != marked, "conversion-target-only metadata moves the heavier underline");
        Hr(view.ReadTextInput(snapshot), "read composition cancellation revision");
        Hr(view.ApplyTextInput(snapshot.revision, {}, EmbeddedTextInputAction::Cancel), "cancel metadata-only composition");
        Check(pixels() == before, "metadata cancellation retires underlines and restores the original surface");
        Check(editor->Snapshot().IsSameRevision(source) && editor->Revision() == revision && editor->GetSelection() == selection && ! editor->CanUndo() &&
                  notifications == 0u && ! view.Controls().GetHwnd(),
              "metadata-only previews preserve source, history, notifications and the no-window host");
        Check(editor->DebugGetState().layoutGeneration == generation, "metadata-only overlay rasterization reuses source geometry");
        editor->SetOnTextChanged({});
        editor->SetOnSelectionChanged({});
    }
}
static void TestRichViewportPreservation(GraphicsFixture& gpu, bool disclosure)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::wstring source;
    for (unsigned paragraph = 0u; paragraph < 200u; ++paragraph)
    {
        if (paragraph == 150u)
            source += L"<details open>\n<summary>Observation details</summary>\nA retained explanation.\n</details>\n\n";
        source += L"Paragraph " + std::to_wstring(paragraph) + L" keeps an observation beside its explanation.\n\n";
    }
    const auto anchor = source.find(disclosure ? L"Observation details" : L"Paragraph 140");
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "viewport shared WARP device");
    for (const auto mode : {ViewMode::Rich, ViewMode::Source})
    {
        if (disclosure && mode == ViewMode::Source)
            continue;
        EmbeddedHost view;
        Hr(view.Attach(graphics), "viewport embedded owner");
        auto owned   = std::make_unique<RichTextEditor>(source);
        auto* editor = owned.get();
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        editor->SetViewMode(mode);
        editor->SetSelection({0u, 0u});
        view.Controls().SetRoot(std::move(owned));
        const auto prepare = [&]
        {
            Hr(PrepareRichEmbeddedContent(view, gpu.width, gpu.height), "prepare viewport fixture");
            gpu.Bind();
            Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose viewport fixture");
        };
        prepare();
        const auto viewport = editor->TryGetTextInputViewportRect();
        const auto caret    = editor->TryGetTextInputCaretRect(view.Controls(), anchor);
        Check(viewport && caret && caret->top > viewport->top + 1000.f, "viewport anchor starts well away from the caret");
        const float inset = disclosure ? 24.f : 0.f;
        Check(editor->SetScrollOffsetDip(caret->top - viewport->top - inset), "scroll to retained viewport anchor");
        prepare();
        const auto before    = editor->GetScrollInfo();
        const auto held      = editor->Snapshot();
        const auto revision  = editor->Revision();
        const auto selection = editor->GetSelection();
        unsigned notifications{};
        editor->SetOnTextChanged([&] { ++notifications; });
        editor->SetOnSelectionChanged([&] { ++notifications; });
        if (disclosure)
        {
            for (const bool expanded : {false, true})
            {
                const auto semantic = editor->GetAcknowledgedSemanticDocument(view.Controls());
                Check(semantic != nullptr, "disclosure has acknowledged semantics");
                const auto& elements = semantic->Elements();
                const auto found     = std::ranges::find_if(elements, [](const auto& value) { return value.role == SemanticRole::Disclosure; });
                Check(found != elements.end(), "fixture contains a disclosure");
                const auto element = static_cast<size_t>(found - elements.begin());
                Check(editor->SetSemanticDisclosure(view.Controls(), semantic, element, expanded), "toggle an off-caret disclosure");
                prepare();
                const auto after = editor->GetScrollInfo();
                std::cout << "viewport disclosure expanded=" << expanded << " before=" << (before ? before->offsetDip : -1.f)
                          << " after=" << (after ? after->offsetDip : -1.f) << " reused=" << editor->DebugGetState().reusedGeometryChunks << '\n';
                Check(before && after && std::abs(before->offsetDip - after->offsetDip) < 1.f,
                      "disclosure preserves its viewport instead of revealing an offscreen caret");
                Check(editor->DebugGetState().reusedGeometryChunks > 0u, "disclosure retains unrelated prepared geometry");
            }
        }
        else
        {
            const auto oldCaret = editor->TryGetTextInputCaretRect(view.Controls(), anchor);
            editor->SetZoom(1.25f); // A second change before preparation retains the original anchor.
            for (const float zoom : {1.5f, .75f, 1.f})
            {
                editor->SetZoom(zoom);
                prepare();
                const auto newCaret = editor->TryGetTextInputCaretRect(view.Controls(), anchor);
                const auto after    = editor->GetScrollInfo();
                std::cout << "viewport zoom mode=" << static_cast<unsigned>(mode) << " zoom=" << zoom << " anchorY=" << (newCaret ? newCaret->top : -1.f)
                          << " offset=" << (after ? after->offsetDip : -1.f) << '\n';
                Check(oldCaret && newCaret && after && after->offsetDip > 1000.f && std::abs(oldCaret->top - newCaret->top) < 1.5f,
                      "zoom retains the first visible source line instead of revealing the caret");
            }
        }
        Check(editor->Snapshot().IsSameRevision(held) && editor->Revision() == revision && editor->GetSelection() == selection && ! editor->CanUndo() &&
                  notifications == 0u && ! view.Controls().GetHwnd(),
              "view changes preserve source, selection, history and notifications without a native window");
        editor->SetOnTextChanged({});
        editor->SetOnSelectionChanged({});
        if (! disclosure)
        {
            editor->SetZoom(1.25f);
            editor->SetSelection({1u, 1u});
            prepare();
            const auto revealed = editor->GetScrollInfo();
            Check(revealed && revealed->offsetDip < 100.f, "an explicit selection supersedes a pending viewport anchor");
            editor->SetZoom(1.5f);
            editor->SetText(L"A replacement document.");
            prepare();
            const auto replacement = editor->GetScrollInfo();
            Check(replacement && replacement->offsetDip == 0.f, "another source revision never inherits a stale viewport anchor");
        }
    }
}

static void TestRichPreparedPageRaster(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "create prepared-page graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "attach prepared-page host");
    auto owned      = std::make_unique<RichTextEditor>(L"# First heading\n\nFirst paragraph.\n\nSecond paragraph.\n\nThird paragraph.\n\nFourth paragraph.\n");
    auto* editor    = owned.get();
    auto appearance = editor->GetAppearance();
    appearance.paddingDip = 0.0f;
    editor->SetAppearance(appearance);
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    editor->SetSelection({2u, 7u});
    view.Controls().SetRoot(std::move(owned));
    Hr(PrepareRichEmbeddedContent(view, 240u, 120u), "prepare immutable export geometry");
    auto* context = view.Controls().GetDeviceContext();
    Check(context != nullptr, "prepared page has a device context");
    const auto properties =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
    const auto readProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                                                        96.0f,
                                                        96.0f);
    wil::com_ptr<ID2D1Bitmap1> page, readback;
    Hr(context->CreateBitmap(D2D1::SizeU(300u, 140u), nullptr, 0u, &properties, page.put()), "create page target");
    Hr(context->CreateBitmap(D2D1::SizeU(300u, 140u), nullptr, 0u, &readProperties, readback.put()), "create page readback");
    wil::com_ptr<ID2D1SolidColorBrush> ink;
    Hr(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), ink.put()), "create page ink");
    wil::com_ptr<ID2D1Image> originalTarget;
    context->GetTarget(originalTarget.put());
    const auto restore = wil::scope_exit([&] noexcept { context->SetTarget(originalTarget.get()); });
    context->SetTarget(page.get());
    context->BeginDraw();
    context->Clear(D2D1::ColorF(D2D1::ColorF::White));
    const auto before    = editor->Snapshot();
    const auto revision  = editor->Revision();
    const auto selection = editor->GetSelection();
    const auto scroll    = editor->GetScrollInfo();
    RichText::ExportDrawing drawing;
    drawing.text = ink.get();
    Check(! editor->TryDrawPreparedPage(view.Controls(), D2D1::RectF(20.f, 5.f, 259.f, 60.f), 0.f, drawing),
          "a page with a different layout width is rejected before drawing");
    size_t observed{};
    {
        const auto oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes, oldFailure = failAllocationAfter;
        const auto reset = wil::scope_exit([=] noexcept
        {
            countAllocations    = oldCounting;
            allocations         = oldCalls;
            allocationBytes     = oldBytes;
            failAllocationAfter = oldFailure;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        failAllocationAfter           = 0u;
        editor->DrawPreparedPage(view.Controls(), D2D1::RectF(20.f, 5.f, 260.f, 60.f), 0.f, drawing);
        drawing.preserveAuthorColors = true;
        drawing.pageText             = D2D1::ColorF(D2D1::ColorF::Black);
        drawing.pageStroke           = D2D1::ColorF(D2D1::ColorF::Black);
        drawing.pageFill             = D2D1::ColorF(D2D1::ColorF::White);
        editor->DrawPreparedPage(view.Controls(), D2D1::RectF(20.f, 75.f, 260.f, 130.f), 60.f, drawing);
        observed = allocations;
    }
    Hr(context->EndDraw(), "draw two clipped pages");
    Check(observed == 0u && editor->Snapshot().IsSameRevision(before) && editor->Revision() == revision && editor->GetSelection() == selection &&
              editor->GetScrollInfo()->offsetDip == scroll->offsetDip && ! editor->CanUndo(),
          "page drawing allocates no C++ storage and preserves source, selection, scroll and history");
    Hr(readback->CopyFromBitmap(nullptr, page.get(), nullptr), "copy page pixels");
    D2D1_MAPPED_RECT mapped{};
    Hr(readback->Map(D2D1_MAP_OPTIONS_READ, &mapped), "map page pixels");
    const auto unmap = wil::scope_exit([&] noexcept { readback->Unmap(); });
    size_t firstInk{}, secondInk{}, outsideInk{};
    for (UINT y = 0u; y < 140u; ++y)
        for (UINT x = 0u; x < 300u; ++x)
        {
            const auto* pixel = mapped.bits + y * mapped.pitch + x * 4u;
            if (pixel[0] == 255u && pixel[1] == 255u && pixel[2] == 255u)
                continue;
            if (x >= 20u && x < 260u && y >= 5u && y < 60u)
                ++firstInk;
            else if (x >= 20u && x < 260u && y >= 75u && y < 130u)
                ++secondInk;
            else
                ++outsideInk;
        }
    Check(firstInk > 0u && secondInk > 0u && outsideInk == 0u, "both page slices draw content strictly inside their requested clips");
    editor->SetText(L"replacement after the prepared page");
    Check(! editor->TryDrawPreparedPage(view.Controls(), D2D1::RectF(20.f, 5.f, 260.f, 60.f), 0.f, drawing),
          "a source change rejects stale prepared-page geometry");
}

static void TestRichPageAuthoredColors(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "create colored-page graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "attach colored-page host");
    // The authored fill must also contrast with the effective stroke: a black
    // fallback stroke on black fill intentionally causes palette rejection.
    auto owned            = std::make_unique<RichTextEditor>(L"```mermaid\nflowchart TD\nA[Alpha]-->B[Beta]\nstyle A fill:#000,stroke:#fff,color:#fff\n```\n");
    auto* editor          = owned.get();
    auto appearance       = editor->GetAppearance();
    appearance.paddingDip = 0.f;
    editor->SetAppearance(appearance);
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    Hr(PrepareRichEmbeddedContent(view, 500u, 400u), "prepare author-colored diagram");
    const auto document = editor->GetAcknowledgedSemanticDocument(view.Controls());
    Check(document && std::ranges::any_of(document->Elements(), [](const auto& element) { return element.role == RichText::SemanticRole::Diagram; }),
          "colored-page fixture admitted a native diagram");
    auto* context = view.Controls().GetDeviceContext();
    Check(context != nullptr, "colored-page fixture has a device context");
    const auto targetProperties =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f);
    const auto readProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                                                        96.f,
                                                        96.f);
    std::array<wil::com_ptr<ID2D1Bitmap1>, 2u> targets, copies;
    for (size_t i = 0u; i != 2u; ++i)
    {
        Hr(context->CreateBitmap(D2D1::SizeU(500u, 400u), nullptr, 0u, &targetProperties, targets[i].put()), "create colored-page target");
        Hr(context->CreateBitmap(D2D1::SizeU(500u, 400u), nullptr, 0u, &readProperties, copies[i].put()), "create colored-page readback");
    }
    wil::com_ptr<ID2D1SolidColorBrush> black, white;
    Hr(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), black.put()), "create colored-page text");
    Hr(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), white.put()), "create colored-page fill");
    wil::com_ptr<ID2D1Image> originalTarget;
    context->GetTarget(originalTarget.put());
    const auto restore = wil::scope_exit([&] noexcept { context->SetTarget(originalTarget.get()); });
    RichText::ExportDrawing drawing;
    drawing.text = drawing.decoration = black.get();
    drawing.fill                      = white.get();
    drawing.pageText = drawing.pageStroke = D2D1::ColorF(D2D1::ColorF::Black);
    drawing.pageFill                      = D2D1::ColorF(D2D1::ColorF::White);
    for (size_t i = 0u; i != 2u; ++i)
    {
        context->SetTarget(targets[i].get());
        context->BeginDraw();
        context->Clear(D2D1::ColorF(D2D1::ColorF::White));
        drawing.preserveAuthorColors = i != 0u;
        editor->DrawPreparedPage(view.Controls(), D2D1::RectF(0.f, 0.f, 500.f, 400.f), 0.f, drawing);
        Hr(context->EndDraw(), "draw colored-page variant");
        Hr(copies[i]->CopyFromBitmap(nullptr, targets[i].get(), nullptr), "copy colored-page variant");
    }
    D2D1_MAPPED_RECT first{}, second{};
    Hr(copies[0]->Map(D2D1_MAP_OPTIONS_READ, &first), "map monochrome page");
    const auto unmapFirst = wil::scope_exit([&] noexcept { copies[0]->Unmap(); });
    Hr(copies[1]->Map(D2D1_MAP_OPTIONS_READ, &second), "map authored-color page");
    const auto unmapSecond = wil::scope_exit([&] noexcept { copies[1]->Unmap(); });
    size_t different{};
    for (UINT y = 0u; y != 400u; ++y)
        for (UINT x = 0u; x != 500u; ++x)
        {
            const auto* a = first.bits + y * first.pitch + x * 4u;
            const auto* b = second.bits + y * second.pitch + x * 4u;
            if (a[0] != b[0] || a[1] != b[1] || a[2] != b[2])
                ++different;
        }
    Check(different > 100u, "the opt-in page palette preserves visible author colors");
}

static void TestRichLabelWakeupAndRetirement(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "create graphics for label wakeup");
    EmbeddedHost view;
    struct CallbackState
    {
        EmbeddedHost* view{};
        bool retire{}, retired{};
        size_t requests{};
    } state{&view};
    EmbeddedCallbacks callbacks{};
    callbacks.context            = &state;
    callbacks.requestPreparation = [](void* context) noexcept
    {
        auto& current = *static_cast<CallbackState*>(context);
        ++current.requests;
        if (current.retire)
        {
            current.retire = false;
            current.view->Controls().SetRoot(nullptr);
            current.retired = true;
        }
    };
    Hr(view.Attach(graphics, callbacks), "attach label wakeup host");
    auto root    = std::make_unique<RichTextEditor>(L"text");
    auto* editor = root.get();
    view.Controls().SetRoot(std::move(root));
    Hr(PrepareRichEmbeddedContent(view, 320u, 120u), "settle label wakeup host");
    const std::array entries{RichText::AccessibilityTextOverride{RichText::AccessibilityTextId::ExitSource, L"Localized exit"}};
    const auto labels = std::make_shared<const RichText::AccessibilityLabels>(entries);
    const auto before = state.requests;
    {
        const auto previous = failAllocationAfter;
        const auto restore  = wil::scope_exit([=] noexcept { failAllocationAfter = previous; });
        failAllocationAfter = 0u;
        editor->SetAccessibilityLabels(labels);
    }
    Check(view.NeedsPreparation() && state.requests == before + 1u, "label setter requests preparation without allocating");
    Hr(PrepareRichEmbeddedContent(view, 320u, 120u), "settle changed labels before retirement");
    state.retire = true;
    editor->SetAccessibilityLabels(nullptr);
    Check(state.retired && ! view.Controls().GetRoot(), "label wakeup callback can retire the editor before the setter returns");
}

static void TestRichTooltipReentrantTransitions(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "create tooltip transition graphics");
    EmbeddedHost view;
    struct CallbackState
    {
        RichTextEditor* editor{};
        unsigned action{};
        bool called{}, failed{};
    } state;
    EmbeddedCallbacks callbacks{};
    callbacks.context            = &state;
    callbacks.requestPreparation = [](void* context) noexcept
    {
        auto& current     = *static_cast<CallbackState*>(context);
        const auto action = std::exchange(current.action, 0u);
        if (! action)
            return;
        current.called = true;
        try
        {
            if (action == 1u)
                current.editor->SetReadOnly(true);
            else if (action == 2u)
                current.editor->SetViewMode(RichText::ViewMode::Rich);
            else
                current.editor->SetText(L"[new](https://example.test \"Current title\")");
        }
        catch (const std::bad_alloc&)
        {
            DxUi::Failure::FailAllocation();
        }
        catch (const std::exception&)
        {
            // ABI callback records failure for the owning test to report.
            current.failed = true;
        }
    };
    Hr(view.Attach(graphics, callbacks), "attach tooltip transition host");
    for (const unsigned action : {1u, 2u, 3u})
    {
        auto root    = std::make_unique<RichTextEditor>(L"[link](https://example.test \"Link title\")");
        state.editor = root.get();
        state.editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::RichMarkdown);
        state.editor->SetMarkdownRenderingEnabled(true);
        state.editor->SetReadOnly(true);
        view.Controls().SetRoot(std::move(root));
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare tooltip transition fixture");
        const auto document = state.editor->GetAcknowledgedSemanticDocument(view.Controls());
        const auto link     = std::ranges::find_if(document->Elements(), [](const auto& element) { return element.role == RichText::SemanticRole::Link; });
        Check(link != document->Elements().end(), "tooltip transition has a link owner");
        const auto bounds = state.editor->GetSemanticElementBounds(view.Controls(), static_cast<std::size_t>(link - document->Elements().begin()));
        Check(bounds.has_value(), "tooltip transition has prepared link bounds");
        view.DispatchPointer({PointerAction::Move, (bounds->left + bounds->right) * .5f, (bounds->top + bounds->bottom) * .5f});
        Check(view.Controls().DebugGetPendingTooltipText() == L"Link title", "tooltip transition selects an authored title");
        Check(view.Controls().DebugAdvanceTooltipDelayForTest(), "tooltip transition makes the owned tooltip visible");
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "settle visible tooltip before callback");
        Check(view.Controls().HasTooltip(), "tooltip transition starts with visible content");
        state.called = state.failed = false;
        state.action                = action;
        if (action == 1u)
            state.editor->SetReadOnly(false);
        else if (action == 2u)
            state.editor->SetViewMode(RichText::ViewMode::Source);
        else
            state.editor->SetText(L"outer replacement");
        Check(state.called && ! state.failed && state.editor->IsReadOnly() && state.editor->GetViewMode() == RichText::ViewMode::Rich,
              "a newer setter in tooltip invalidation wins over the outer read-only, mode or source transition");
        const std::wstring_view expected = action == 3u ? L"[new](https://example.test \"Current title\")" : L"[link](https://example.test \"Link title\")";
        Check(state.editor->GetText() == expected && ! state.editor->CanUndo(), "reentrant tooltip transitions preserve the newest source and history");
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "settle reentrant tooltip transition");
    }
    auto root    = std::make_unique<RichTextEditor>(L"[link](https://example.test \"Retired title\")");
    state.editor = root.get();
    state.editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::RichMarkdown);
    state.editor->SetMarkdownRenderingEnabled(true);
    state.editor->SetTooltipText(L"General tooltip");
    view.Controls().SetRoot(std::move(root));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare generic-to-rich tooltip transition");
    const auto document = state.editor->GetAcknowledgedSemanticDocument(view.Controls());
    const auto link     = std::ranges::find_if(document->Elements(), [](const auto& element) { return element.role == RichText::SemanticRole::Link; });
    Check(link != document->Elements().end(), "generic tooltip fixture has a link owner");
    const auto bounds = state.editor->GetSemanticElementBounds(view.Controls(), static_cast<std::size_t>(link - document->Elements().begin()));
    Check(bounds.has_value(), "generic tooltip fixture has prepared bounds");
    const PointerEvent move{PointerAction::Move, (bounds->left + bounds->right) * .5f, (bounds->top + bounds->bottom) * .5f};
    view.DispatchPointer(move);
    Check(view.Controls().DebugGetPendingTooltipText() == L"General tooltip", "editable hover starts with the generic tooltip");
    Check(view.Controls().DebugAdvanceTooltipDelayForTest(), "make the generic tooltip visible before transition");
    state.editor->SetReadOnly(true);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "settle read-only generic tooltip fixture");
    Check(view.Controls().HasTooltip(), "generic tooltip remains visible before the rich target changes");
    state.called = state.failed = false;
    state.action                = 3u;
    view.DispatchPointer(move);
    Check(state.called && ! state.failed && state.editor->GetText() == L"[new](https://example.test \"Current title\")",
          "generic tooltip clear callback replaces the candidate rich document");
    Check(view.Controls().DebugGetPendingTooltipText() != L"Retired title", "post-clear resolution never publishes the retired candidate title");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "settle replacement after tooltip clear");
    view.DispatchPointer(move);
    Check(view.Controls().DebugGetPendingTooltipText() == L"Current title", "subsequent hover resolves the current document's authored title");
}

static void TestRichSourceDragGesture(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "create source-drag graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "attach source-drag host");
    auto root    = std::make_unique<RichTextEditor>(L"one two three");
    auto* editor = root.get();
    view.Controls().SetRoot(std::move(root));
    view.Controls().SetFocusControl(editor);
    editor->SetSelection({4u, 7u});
    Hr(PrepareRichEmbeddedContent(view, 320u, 120u), "prepare source-drag text");
    EmbeddedTextInputSnapshot snapshot;
    Check(view.ReadTextInput(snapshot) == S_OK, "read source-drag text state");
    D2D1_RECT_F bounds{};
    bool clipped = true;
    Check(view.GetTextInputRangeBounds(snapshot.revision, 4u, 7u, bounds, clipped) == S_OK && ! clipped, "selected source has visible geometry");
    bool invoked = false;
    editor->SetOnSourceDragRequested([&](const RichTextSourceMove& move, std::wstring_view text)
    { invoked = text == L"two" && editor->MoveSelectedSource(move, 13u); });
    const float x = (bounds.left + bounds.right) * 0.5f, y = (bounds.top + bounds.bottom) * 0.5f;
    Check(view.DispatchPointer({PointerAction::Down, x, y}), "press inside selection begins a possible source drag");
    Check(editor->GetSelection() == RichText::Selection{4u, 7u}, "press preserves selection until drag threshold or release");
    Check(view.DispatchPointer({PointerAction::Move, x + 40.0f, y}), "crossing threshold invokes source-drag transport");
    Check(invoked && editor->GetText() == L"one  threetwo" && editor->Undo() && editor->GetText() == L"one two three",
          "transport performs one exact undoable move");
    editor->SetSelection({4u, 7u});
    Hr(PrepareRichEmbeddedContent(view, 320u, 120u), "prepare stale source-drag gesture");
    invoked = false;
    // This is a separate primary gesture, independent of the first drag's
    // platform double-click interval. Exercise the press guard before a
    // preparation turn can cancel capture for its own independent reason.
    Check(editor->OnMouseDown(view.Controls(), {x, y}, false, 0u) && view.Controls().GetCapturedControl() == editor,
          "begin another possible source drag with owned capture");
    editor->SetSelection({0u, 0u});
    editor->SetSelection({4u, 7u});
    Check(view.DispatchPointer({PointerAction::Move, x + 40.0f, y}) && ! invoked && editor->GetText() == L"one two three" &&
              ! view.Controls().GetCapturedControl(),
          "selection changed away and back cancels the original press");
    Hr(PrepareRichEmbeddedContent(view, 320u, 120u), "prepare after cancelled stale source drag");
}

static void TestRichMiddleAutoScroll(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "create auto-scroll graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "attach auto-scroll host");
    std::wstring source(0u, L' ');
    for (unsigned i = 0u; i != 100u; ++i)
        source.append(L"a long source line\n");
    auto root    = std::make_unique<RichTextEditor>(source);
    auto* editor = root.get();
    view.Controls().SetRoot(std::move(root));
    Hr(PrepareRichEmbeddedContent(view, 320u, 120u), "prepare auto-scroll text");
    const auto initial = editor->GetScrollInfo();
    Check(initial && initial->contentHeightDip > initial->viewportHeightDip, "fixture has a vertical scroll extent");
    Check(view.DispatchPointer({PointerAction::MiddleDown, 40.0f, 40.0f}), "middle click starts auto-scroll");
    Check(view.DispatchPointer({PointerAction::MiddleUp, 40.0f, 40.0f}), "middle release retains the auto-scroll gesture");
    Check(view.DispatchPointer({PointerAction::Move, 40.0f, 90.0f}), "pointer displacement requests animation");
    for (uint64_t tick = 100u; tick != 180u; tick += 16u)
        if (view.NeedsAnimation())
            view.AdvanceAnimation(tick);
    const auto after = editor->GetScrollInfo();
    Check(after && after->offsetDip > initial->offsetDip && editor->GetText() == source && ! editor->CanUndo(),
          "ticks scroll without changing source or history");
    Check(view.DispatchPointer({PointerAction::MiddleDown, 40.0f, 90.0f}), "second middle click cancels auto-scroll");
    Check(! view.Controls().GetCapturedControl(), "cancelling releases the retained capture");
    Check(editor->SetScrollOffsetDip(0.0f), "reset auto-scroll position for overlapping gesture checks");
    Check(view.DispatchPointer({PointerAction::Down, 40.0f, 40.0f}), "primary press owns the selection gesture");
    Check(! view.DispatchPointer({PointerAction::MiddleDown, 40.0f, 40.0f}), "middle press cannot overlay an active primary gesture");
    Check(view.DispatchPointer({PointerAction::Up, 40.0f, 40.0f}), "primary release completes the original gesture");
    Check(! view.Controls().GetCapturedControl(), "primary release retires capture");
    const auto released = editor->GetScrollInfo()->offsetDip;
    static_cast<void>(view.DispatchPointer({PointerAction::Move, 40.0f, 100.0f}));
    static_cast<void>(editor->Tick(view.Controls(), 200u));
    static_cast<void>(editor->Tick(view.Controls(), 250u));
    Check(editor->GetScrollInfo()->offsetDip == released && editor->GetText() == source && ! editor->CanUndo(),
          "rejected overlapping middle press cannot scroll after primary release");
    Check(view.DispatchPointer({PointerAction::MiddleDown, 40.0f, 40.0f}), "middle gesture starts after the primary release");
    Check(view.DispatchPointer({PointerAction::Down, 40.0f, 40.0f}), "primary press cancels the earlier middle gesture");
    Check(view.DispatchPointer({PointerAction::Cancel}), "capture cancellation retires the replacement primary gesture");
    const auto cancelled = editor->GetScrollInfo()->offsetDip;
    static_cast<void>(view.DispatchPointer({PointerAction::Move, 40.0f, 100.0f}));
    static_cast<void>(editor->Tick(view.Controls(), 300u));
    static_cast<void>(editor->Tick(view.Controls(), 350u));
    Check(! view.Controls().GetCapturedControl() && editor->GetScrollInfo()->offsetDip == cancelled,
          "primary takeover and capture cancellation do not leave auto-scroll running");
}

static void TestRichSourceMoveTransaction()
{
    using namespace DxUi;
    RichTextEditor editor(L"one two three");
    editor.SetSelection({4u, 7u});
    const auto move = editor.BeginSourceMove();
    Check(move && editor.MoveSelectedSource(*move, 13u), "move selected source to end");
    Check(editor.GetText() == L"one  threetwo" && editor.GetSelection() == RichText::Selection{10u, 13u},
          "move preserves exact source and selects the moved span");
    Check(editor.Undo() && editor.GetText() == L"one two three", "one undo restores the whole move");
    editor.SetSelection({4u, 7u});
    const auto stale = editor.BeginSourceMove();
    editor.SetSelection({0u, 0u});
    editor.SetSelection({4u, 7u});
    Check(stale && ! editor.MoveSelectedSource(*stale, 0u) && editor.GetText() == L"one two three", "restoring the same selection cannot revive a stale move");
    const auto cancelled = editor.BeginSourceMove();
    editor.CancelSourceMove();
    Check(cancelled && ! editor.MoveSelectedSource(*cancelled, 0u), "cancelling a gesture retires its token");
    const auto current = editor.BeginSourceMove();
    Check(current && ! editor.MoveSelectedSource(*current, 5u), "a destination inside the source selection is a no-op");
    Check(editor.MoveSelectedSource(*current, 0u) && editor.GetText() == L"twoone  three", "move to an earlier source position");
    editor.SetReadOnly(true);
    Check(! editor.BeginSourceMove(), "read-only editor does not begin a move");
    editor.SetReadOnly(false);
    editor.SetText(L"one two three");
    editor.SetSelection({4u, 7u});
    const auto external = editor.BeginSourceMove();
    Check(external && editor.CompleteExternalSourceMove(*external) && editor.GetText() == L"one  three" && editor.Undo() &&
              editor.GetText() == L"one two three",
          "confirmed external move deletes source in one undoable transaction");
    editor.SetSelection({4u, 7u});
    const auto staleExternal = editor.BeginSourceMove();
    editor.SetSelection({0u, 0u});
    editor.SetSelection({4u, 7u});
    Check(staleExternal && ! editor.CompleteExternalSourceMove(*staleExternal) && editor.GetText() == L"one two three",
          "stale external completion leaves the original source intact");
}

static void TestRichTripleClickParagraph(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "create triple-click graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "attach triple-click host");
    auto root    = std::make_unique<RichTextEditor>(L"alpha beta\n\nsecond line");
    auto* editor = root.get();
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    editor->SetViewMode(RichText::ViewMode::Rich);
    view.Controls().SetRoot(std::move(root));
    view.Controls().SetFocusControl(editor);
    Hr(PrepareRichEmbeddedContent(view, 320u, 120u), "prepare triple-click paragraph");
    EmbeddedTextInputSnapshot snapshot;
    Check(view.ReadTextInput(snapshot) == S_OK, "read triple-click text state");
    D2D1_RECT_F bounds{};
    bool clipped = true;
    Check(view.GetTextInputRangeBounds(snapshot.revision, 0u, 5u, bounds, clipped) == S_OK && ! clipped, "first word has visible geometry");
    const float x = (bounds.left + bounds.right) * 0.5f, y = (bounds.top + bounds.bottom) * 0.5f;
    for (unsigned click = 0u; click != 3u; ++click)
    {
        Check(view.DispatchPointer({PointerAction::Down, x, y}), "paragraph click is dispatched");
        static_cast<void>(view.DispatchPointer({PointerAction::Up, x, y}));
        Hr(PrepareRichEmbeddedContent(view, 320u, 120u), "prepare after paragraph click");
    }
    Check(editor->GetSelection() == RichText::Selection{0u, 10u}, "third click selects the first semantic paragraph only");
    Check(editor->Snapshot().Text() == L"alpha beta\n\nsecond line", "triple click preserves source");
}

__declspec(noinline) static void RichPreparedWordFaultOrigin(DxUi::RichTextEditor& editor, DxUi::EmbeddedHost& view)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)editor.GetPreparedCaretWordSourceRanges(view.Controls());
}
static void TestRichPreparedWordFailureAtomicity(GraphicsFixture& gpu, unsigned fatalScenario = 0u)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "create graphics for caret-word fault tests");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "attach caret-word fault host");
    auto root    = std::make_unique<RichTextEditor>(L"**word**");
    auto* editor = root.get();
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(root));
    for (const auto mode : {RichText::ViewMode::Source, RichText::ViewMode::Rich})
    {
        const unsigned scenario = mode == RichText::ViewMode::Source ? 1u : 2u;
        if (fatalScenario != 0u && fatalScenario != scenario)
            continue;
        editor->SetViewMode(mode);
        editor->SetSelection({3u, 3u});
        Hr(PrepareRichEmbeddedContent(view, 320u, 120u), "prepare caret-word fault fixture");
        if (fatalScenario != 0u)
        {
            RichPreparedWordFaultOrigin(*editor, view);
            return;
        }
        const auto source    = editor->Snapshot();
        const auto selection = editor->GetSelection();
        const auto revision  = editor->Revision();
        const auto undo = editor->CanUndo(), redo = editor->CanRedo();
        size_t requests{};
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
            const auto result             = editor->GetPreparedCaretWordSourceRanges(view.Controls());
            countAllocations              = false;
            requests                      = allocations;
            Check(result.has_value(), "prepared caret-word query succeeds when allocation is available");
        }
        Check(requests > 0u && requests < 64u, "caret-word query has a bounded set of allocation points");
        Check(DxUiFailureTests::ExpectFatalChild(scenario == 1u ? L"embedded-source-word-query" : L"embedded-rich-word-query"),
              "caret-word query allocation terminates in its owned child");
        Check(editor->GetPreparedCaretWordSourceRanges(view.Controls()).has_value() && editor->Snapshot().IsSameRevision(source) &&
                  editor->GetSelection() == selection && editor->Revision() == revision && editor->CanUndo() == undo && editor->CanRedo() == redo,
              "caret-word query preserves source, selection, revision and history");
    }
}

__declspec(noinline) static void RichAppearanceConstructionFaultOrigin()
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    [[maybe_unused]] DxUi::RichTextEditorAppearance appearance;
}
__declspec(noinline) static void RichEditorConstructionFaultOrigin()
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    [[maybe_unused]] DxUi::RichTextEditor editor;
}
static void TestRichAppearanceConstructionFailureAtomicity()
{
    static_assert(! std::is_nothrow_default_constructible_v<DxUi::RichTextEditorAppearance>);
    size_t requests{};
    bool valid{};
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
        {
            DxUi::RichTextEditorAppearance appearance;
            valid = appearance.bodyFontFamily.empty() && appearance.codeFontFamily.empty() && appearance.mathFontFamily.empty();
        }
        countAllocations = false;
        requests         = allocations;
    }
    Check(valid && requests <= 8u, "default appearance retains empty font overrides with a bounded construction schedule");
    if (requests != 0u)
        Check(DxUiFailureTests::ExpectFatalChild(L"embedded-appearance-construction-allocation"),
              "appearance construction allocation terminates in its owned child");
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-editor-construction-allocation"), "editor construction allocation terminates in its owned child");
}
static void TestHeadingAdmissionNoAllocation()
{
    using namespace DxUi::RichText;
    std::wstring source(0u, L' ');
    for (size_t depth = 0u; depth < 66u; ++depth)
    {
        source.append(depth * 2u, L' ');
        source += L"- x\n";
    }
    const auto caret = source.rfind(L"- x") + 2u;
    MarkdownParser parser;
    parser.Begin(source, 1u, MarkdownProfile::RichMarkdown);
    while (parser.HasPendingPreparation())
        parser.Advance();
    const auto syntax = parser.Published();
    bool available    = false;
    {
        const auto previous = failAllocationAfter;
        const auto restore  = wil::scope_exit([=] noexcept { failAllocationAfter = previous; });
        failAllocationAfter = 0u;
        available           = CanSetHeading(*syntax, {caret, caret}, 2u);
    }
    Check(available, "heading admission proves deeply nested prefixes without allocation");
}

template <typename Operation> __declspec(noinline) static void RichInputFaultOrigin(Operation&& operation)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)operation();
}
static void TestRichInputAllocationFailureAtomicity(unsigned fatalScenario = 0u)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    for (unsigned operation = 0u; operation < 4u; ++operation)
    {
        if (fatalScenario != 0u && fatalScenario != operation + 1u)
            continue;
        ControlHost host;
        RichTextEditor editor(L"alpha beta");
        editor.SetViewMode(ViewMode::Source);
        editor.SetSelection({5u, 5u});
        unsigned notifications = 0u;
        editor.SetOnTextChanged([&] { ++notifications; });
        if (operation == 3u)
            Check(editor.OnChar(host, wchar_t(0xd83d), 0u), "input fixture retains the pending lead surrogate");
        const auto revision = editor.Revision();
        const auto act      = [&]
        {
            switch (operation)
            {
                case 0u: return editor.OnChar(host, L'X', 0u);
                case 1u: return editor.OnKeyDown(host, L'B', MK_CONTROL);
                case 2u: return editor.OnKeyDown(host, VK_BACK, 0u);
                default: return editor.OnChar(host, wchar_t(0xde00), 0u);
            }
        };
        if (fatalScenario != 0u)
        {
            RichInputFaultOrigin(act);
            return;
        }
        const wchar_t* child = operation == 0u   ? L"embedded-input-character-allocation"
                               : operation == 1u ? L"embedded-input-command-allocation"
                               : operation == 2u ? L"embedded-input-backspace-allocation"
                                                 : L"embedded-input-surrogate-allocation";
        Check(DxUiFailureTests::ExpectFatalChild(child), "input allocation terminates in its owned child");
        const auto oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        const bool handled            = act();
        countAllocations              = oldCounting;
        const auto observed           = allocations;
        allocations                   = oldCalls;
        allocationBytes               = oldBytes;
        Check(observed > 0u && observed < 2048u, "native input has a bounded observed allocation schedule");
        Check(handled && editor.Revision() != revision && editor.CanUndo() && notifications == 1u && IsValidSourceUtf16(editor.GetText()),
              "successful input commits one complete Unicode transaction");
        Check(editor.Undo() && editor.GetText() == L"alpha beta" && ! editor.CanUndo(), "one undo restores the complete input transaction");
    }
}

__declspec(noinline) static void RichFileDropTransportFaultOrigin(IDataObject* data, DxUi::TextClipboardContent& content)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)DxUi::ReadDataObjectFilePaths(data, content);
}
static void TestRichFileDropTransportAllocationFailures(bool fatalChild = false)
{
    using namespace DxUi;
    auto data = Microsoft::WRL::Make<RichFileDropTests::DropDataObject>();
    Check(data != nullptr, "allocate private file-drop data object");
    constexpr wchar_t list[] = L"C:\\docs\\one.png\0C:\\docs\\two.pdf\0";
    data->bytes              = RichImagePathTests::DropBuffer(std::wstring_view(list, std::size(list)));
    if (fatalChild)
    {
        TextClipboardContent content;
        RichFileDropTransportFaultOrigin(data.Get(), content);
        return;
    }
    TextClipboardContent measured;
    const auto oldCounting = countAllocations;
    const auto oldCalls = allocations, oldBytes = allocationBytes;
    allocations = allocationBytes = 0u;
    countAllocations              = true;
    const auto measuredResult     = ReadDataObjectFilePaths(data.Get(), measured);
    countAllocations              = oldCounting;
    const auto observed           = allocations;
    allocations                   = oldCalls;
    allocationBytes               = oldBytes;
    Check(measuredResult == S_OK && measured.filePaths.size() == 2u && observed > 0u && observed < 256u,
          "file-drop transport has an observed bounded allocation schedule");
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-file-drop-transport-allocation"), "file-drop transport allocation terminates in its owned child");
    TextClipboardContent content;
    const auto releases = data->releases;
    Check(ReadDataObjectFilePaths(data.Get(), content) == S_OK && content.filePaths.size() == 2u && data->releases == releases + 1u,
          "file-drop transport returns both files and releases its acquired medium once");
}
static void TestPreparedCommandStateAllocationFailures(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "prepared command state fixture graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "attach prepared command state fixture");
    auto owned   = std::make_unique<RichTextEditor>(L"word");
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    editor->SetSelection({0u, 4u});
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare command state fixture");
    MarkdownCommandRequest previous;
    previous.command = MarkdownCommand::ToggleStyle;
    MarkdownCommandRequest next;
    next.style       = MarkdownTextStyle::Emphasis;
    next.text        = L"x";
    next.destination = L"https://example.invalid/target";
    next.title       = L"title";
    Check(editor->PrepareMarkdownCommandState(previous), "prepare old command-state cache");
    const auto source    = editor->Snapshot();
    const auto selection = editor->GetSelection();
    const auto revision  = editor->Revision();
    for (const auto command : {MarkdownCommand::ToggleStyle,
                               MarkdownCommand::SetHeading,
                               MarkdownCommand::SetList,
                               MarkdownCommand::SetQuote,
                               MarkdownCommand::SetLink,
                               MarkdownCommand::RemoveLink,
                               MarkdownCommand::InsertImage,
                               MarkdownCommand::SetCodeBlock,
                               MarkdownCommand::InsertMath,
                               MarkdownCommand::InsertTable,
                               MarkdownCommand::SetTableCell})
    {
        next.command = command;
        size_t requests{};
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
            Check(editor->PrepareMarkdownCommandState(next), "measure exact command-state preparation");
            countAllocations = false;
            requests         = allocations;
        }
        Check(requests > 0u && requests < 2048u, "prepared command state has finite allocation points");
        const auto expectedState = editor->QueryMarkdownCommandState(next);
        Check(expectedState && expectedState->available.has_value(), "measured prepared request has exact availability");
        Check(editor->PrepareMarkdownCommandState(previous) && editor->PrepareMarkdownCommandState(next) && editor->Snapshot().IsSameRevision(source) &&
                  editor->GetSelection() == selection && editor->Revision() == revision && ! editor->CanUndo(),
              "switching prepared command states preserves source, selection and history");
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = 0u;
            const auto result     = editor->QueryMarkdownCommandState(next);
            Check(result && result->available == expectedState->available, "prepared command-state lookup allocates no storage");
        }
        std::cout << "prepared command-state command=" << static_cast<unsigned>(command) << " allocation points=" << requests << '\n';
    }
}

static void TestPreparedCommandStateComplexAllocationFailures(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "complex prepared state fixture graphics");
    const auto sweep = [&](std::wstring_view spelling, std::wstring_view selected, MarkdownCommand command)
    {
        EmbeddedHost view;
        Hr(view.Attach(graphics), "attach complex prepared state fixture");
        auto owned   = std::make_unique<RichTextEditor>(std::wstring(spelling));
        auto* editor = owned.get();
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        view.Controls().SetRoot(std::move(owned));
        const auto caret = spelling.find(selected);
        Check(caret != spelling.npos, "complex prepared state fixture has its selected text");
        editor->SetSelection({caret, caret});
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare complex command state fixture");
        MarkdownCommandRequest previous;
        previous.command = MarkdownCommand::InsertEscapedText;
        previous.text    = L"prior";
        MarkdownCommandRequest next;
        next.command     = command;
        next.text        = L"replacement";
        next.destination = L"https://example.invalid/new";
        next.title       = L"updated";
        Check(editor->PrepareMarkdownCommandState(previous), "prepare prior complex command state");
        const auto oldState = editor->QueryMarkdownCommandState(previous);
        Check(oldState && oldState->available.has_value(), "prior complex command state is exact");
        const auto source    = editor->Snapshot();
        const auto selection = editor->GetSelection();
        const auto revision  = editor->Revision();
        size_t requests{};
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
            Check(editor->PrepareMarkdownCommandState(next), "measure complex command preparation");
            countAllocations = false;
            requests         = allocations;
        }
        const auto expected = editor->QueryMarkdownCommandState(next);
        Check(requests > 0u && requests < 2048u && expected && expected->available.has_value(), "complex command preparation has bounded observed allocations");
        std::cout << "complex prepared command=" << static_cast<unsigned>(command) << " allocation points=" << requests << std::endl;
        Check(editor->PrepareMarkdownCommandState(previous), "restore prior complex command state");
        const auto actual = editor->QueryMarkdownCommandState(previous);
        Check(actual && actual->available == oldState->available && editor->PrepareMarkdownCommandState(next) && editor->Snapshot().IsSameRevision(source) &&
                  editor->GetSelection() == selection && editor->Revision() == revision && ! editor->CanUndo(),
              "complex command state switches preserve the source transaction");
        const auto actualNext = editor->QueryMarkdownCommandState(next);
        Check(actualNext && actualNext->available == expected->available, "complex command retry retains exact availability");
    };
    constexpr std::wstring_view anchor = L"<a href=\"https://example.invalid\">label</a>";
    constexpr std::wstring_view table  = L"| a | b |\n|---|---|\n| x | y |\n";
    sweep(anchor, L"label", MarkdownCommand::SetLink);
    sweep(anchor, L"label", MarkdownCommand::RemoveLink);
    sweep(table, L"x", MarkdownCommand::InsertTableRow);
    sweep(table, L"x", MarkdownCommand::SetTableCell);
}

static void TestTableCommandPreparationAllocationFailures(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "table command failure fixture graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "attach table command failure fixture");
    const std::wstring source(L"| a | b |\n| :- | -: |\n| x | y |\n| m | n |\n");
    auto owned   = std::make_unique<RichTextEditor>(source);
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    const auto caret = source.find(L"y");
    editor->SetSelection({caret, caret});
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare table command failure fixture");
    MarkdownCommandRequest previous;
    previous.command    = MarkdownCommand::InsertEscapedText;
    previous.text       = L"prior";
    const auto snapshot = editor->Snapshot();
    const auto revision = editor->Revision();
    for (const auto command : {MarkdownCommand::SetTableAlignment,
                               MarkdownCommand::MoveTableRowUp,
                               MarkdownCommand::MoveTableRowDown,
                               MarkdownCommand::MoveTableColumnLeft,
                               MarkdownCommand::MoveTableColumnRight,
                               MarkdownCommand::InsertFootnote})
    {
        const auto selected = command == MarkdownCommand::MoveTableColumnRight ? source.find(L"x") : caret;
        editor->SetSelection({selected, selected});
        const auto selection = editor->GetSelection();
        Check(editor->PrepareMarkdownCommandState(previous), "prepare prior table state");
        const auto oldState = editor->QueryMarkdownCommandState(previous);
        Check(oldState && oldState->available.has_value(), "prior table state is exact");
        MarkdownCommandRequest request;
        request.command        = command;
        request.tableAlignment = MarkdownAlignment::Center;
        request.text           = L"note";
        size_t requests{};
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
            Check(editor->PrepareMarkdownCommandState(request), "measure table command preparation");
            countAllocations = false;
            requests         = allocations;
        }
        const auto expected = editor->QueryMarkdownCommandState(request);
        Check(expected && expected->available == std::optional<bool>{true} && requests > 0u && requests < 2048u,
              "table command fixture measures an available bounded preparation");
        std::cout << "table command=" << static_cast<unsigned>(command) << " allocation points=" << requests << std::endl;
        Check(editor->PrepareMarkdownCommandState(previous), "restore prior table state");
        const auto actual = editor->QueryMarkdownCommandState(previous);
        Check(actual && actual->available == oldState->available && editor->PrepareMarkdownCommandState(request) &&
                  editor->Snapshot().IsSameRevision(snapshot) && editor->GetSelection() == selection && editor->Revision() == revision && ! editor->CanUndo(),
              "table preparation preserves source, selection and history");
    }
    editor->SetText(L"");
    editor->SetSelection({0u, 0u});
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare delimited table allocation fixture");
    const auto emptySnapshot = editor->Snapshot();
    MarkdownCommandRequest delimited;
    delimited.command          = MarkdownCommand::InsertTable;
    delimited.tableInputFormat = MarkdownTableInputFormat::Csv;
    delimited.text             = L"header,value\n\"a,b\",\"line\nnext\"";
    Check(editor->PrepareMarkdownCommandState(previous), "prepare prior delimited table state");
    const auto previousState = editor->QueryMarkdownCommandState(previous);
    size_t delimitedRequests{};
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
        Check(editor->PrepareMarkdownCommandState(delimited), "measure delimited table command preparation");
        countAllocations  = false;
        delimitedRequests = allocations;
    }
    Check(delimitedRequests > 0u && delimitedRequests < 2048u, "delimited table preparation has bounded allocation points");
    Check(editor->PrepareMarkdownCommandState(previous), "restore prior delimited table state");
    const auto actual = editor->QueryMarkdownCommandState(previous);
    Check(previousState && actual && actual->available == previousState->available && editor->PrepareMarkdownCommandState(delimited) &&
              editor->Snapshot().IsSameRevision(emptySnapshot) && editor->GetSelection() == Selection{0u, 0u} && ! editor->CanUndo(),
          "delimited table preparation preserves source, selection and history");
}

__declspec(noinline) static void RichFileDropTransactionFaultOrigin(DxUi::EmbeddedHost& view, D2D1_POINT_2F point, IDataObject& data)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)view.CommitFileDrop(point, data);
}
static void TestRichFileDropTransactionAllocationFailures(GraphicsFixture& gpu, bool fatalChild = false)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "file-drop fault fixture graphics");
    auto data = Microsoft::WRL::Make<RichFileDropTests::DropDataObject>();
    Check(data != nullptr, "file-drop fault fixture data object");
    constexpr wchar_t list[] = L"C:\\docs\\one.png\0C:\\docs\\two.pdf\0";
    data->bytes              = RichImagePathTests::DropBuffer(std::wstring_view(list, std::size(list)));
    const auto exercise      = [&]
    {
        EmbeddedHost view;
        Hr(view.Attach(graphics), "attach file-drop fault fixture");
        auto owned   = std::make_unique<RichTextEditor>(L"alpha");
        auto* editor = owned.get();
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        editor->SetDocumentFilePath(L"C:\\docs\\note.md");
        view.Controls().SetRoot(std::move(owned));
        editor->SetSelection({5u, 0u});
        Hr(PrepareRichEmbeddedContent(view, 640u, 240u), "prepare file-drop fault fixture");
        const auto rect = editor->TryGetTextInputCaretRect(view.Controls(), 2u);
        Check(rect.has_value(), "file-drop fault fixture caret");
        const auto point = D2D1::Point2F(rect->left + .01f, (rect->top + rect->bottom) * .5f);
        if (fatalChild)
        {
            RichFileDropTransactionFaultOrigin(view, point, *data.Get());
            return size_t{};
        }
        const auto oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        const bool committed          = view.CommitFileDrop(point, *data.Get());
        const auto observed           = allocations;
        countAllocations              = oldCounting;
        allocations                   = oldCalls;
        allocationBytes               = oldBytes;
        Check(committed && editor->GetText() == L"al![one](one.png)\r\n\r\n[two\\.pdf](two.pdf)pha",
              "successful file-drop transaction inserts both references");
        return observed;
    };
    const auto observed = exercise();
    if (fatalChild)
        return;
    Check(observed > 0u && observed < 2048u, "file-drop transaction has an observed bounded allocation schedule");
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-file-drop-transaction-allocation"), "file-drop transaction allocation terminates in its owned child");
}

// The paired benchmark also compiles this common fixture against its older
// frozen SDK. Keep the new API references dependent; ordinary qualification
// of this revision must execute the supported branch below.
__declspec(noinline) static void RichHistoryUndoFaultOrigin()
{
    DxUi::RichText::Document history(L"x");
    Check(history.ReplaceSelection(L"y"), "seed history for undo allocation fault");
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)history.Undo();
}
template <typename Operation> __declspec(noinline) static void RichSnapshotAdoptionFaultOrigin(Operation&& operation)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    operation();
}
template <typename Parser = DxUi::RichText::MarkdownParser, typename Editor = DxUi::RichTextEditor, typename Snapshot = DxUi::RichText::SourceSnapshot>
static void TestReadOnlyParsedSnapshotAllocationFailures(unsigned fatalScenario = 0u)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    if constexpr (requires(Parser& parser, Editor& editor, const SourceSnapshot& source, const std::shared_ptr<const MarkdownSyntax>& syntax) {
                      parser.TryAdoptParsedSnapshot(source, 1u, syntax, MarkdownProfile::RichMarkdown);
                      editor.TryAdoptReadOnlyMarkdownSyntax(source, syntax);
                  })
    {
        const std::wstring text = L"# Shared **世界 😀**\r\n\r\n- [x] task &amp; [link](https://example.test)\r\n";
        Document document(text);
        const Snapshot source = document.Snapshot();
        const auto finish     = [](Parser& parser)
        {
            while (parser.HasPendingPreparation())
                parser.Advance(17u);
        };
        Parser ownerParser;
        ownerParser.Begin(source, 40u, MarkdownProfile::RichMarkdown);
        finish(ownerParser);
        const auto owner = ownerParser.Published();
        Parser replacementParser;
        replacementParser.Begin(source, 41u, MarkdownProfile::RichMarkdown);
        finish(replacementParser);
        const auto replacement = replacementParser.Published();
        const auto observe     = [](auto&& operation)
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
            return allocations;
        };
        const auto pendingParser = [&]
        {
            Parser parser;
            parser.Begin(L"previous publication", 1u);
            finish(parser);
            parser.Begin(L"preserved pending request", 2u);
            parser.Advance(1u);
            return parser;
        };
        auto counted = pendingParser();
        const auto parserRequests =
            observe([&] { Check(counted.TryAdoptParsedSnapshot(source, 3u, owner, MarkdownProfile::RichMarkdown), "count parser adoption"); });
        Check(parserRequests > 0u && parserRequests < 64u, "parsed adoption has a bounded observed allocation schedule");
        {
            auto parser         = pendingParser();
            const auto previous = parser.Published();
            if (fatalScenario == 1u)
            {
                RichSnapshotAdoptionFaultOrigin([&] { (void)parser.TryAdoptParsedSnapshot(source, 3u, owner, MarkdownProfile::RichMarkdown); });
                return;
            }
            if (fatalScenario == 0u)
                Check(DxUiFailureTests::ExpectFatalChild(L"embedded-parser-adoption-allocation"), "parser adoption allocation terminates in its owned child");
            Check(parser.TryAdoptParsedSnapshot(source, 3u, owner, MarkdownProfile::RichMarkdown) && parser.Published() != previous,
                  "parser adopts the complete parsed source");
        }
        const auto makeEditor = [&]
        {
            auto editor = std::make_unique<Editor>();
            Check(editor->ReplaceSelection(text), "seed target undo history");
            editor->SetSelection({text.size(), 0u});
            editor->SetViewMode(ViewMode::Source);
            editor->SetReadOnly(true);
            editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
            return editor;
        };
        auto countedEditor = makeEditor();
        const auto editorRequests =
            observe([&] { Check(countedEditor->TryAdoptReadOnlyMarkdownSyntax(countedEditor->Snapshot(), owner) == S_OK, "count editor adoption"); });
        Check(editorRequests > 0u && editorRequests < 64u, "editor reuse has a bounded observed allocation schedule");
        {
            auto editor = makeEditor();
            Check(editor->TryAdoptReadOnlyMarkdownSyntax(editor->Snapshot(), replacement) == S_OK, "seed prior completed target publication");
            const auto before    = editor->Snapshot();
            const auto revision  = editor->Revision();
            const auto selection = editor->GetSelection();
            unsigned notifications{};
            editor->SetOnTextChanged([&] { ++notifications; });
            editor->SetOnSelectionChanged([&] { ++notifications; });
            if (fatalScenario == 2u)
            {
                RichSnapshotAdoptionFaultOrigin([&] { (void)editor->TryAdoptReadOnlyMarkdownSyntax(before, owner); });
                return;
            }
            if (fatalScenario == 0u)
                Check(DxUiFailureTests::ExpectFatalChild(L"embedded-editor-adoption-allocation"), "editor adoption allocation terminates in its owned child");
            Check(editor->TryAdoptReadOnlyMarkdownSyntax(before, owner) == S_OK && editor->GetMarkdownSyntax() && editor->Snapshot().IsSameRevision(before) &&
                      editor->Revision() == revision && editor->GetSelection() == selection && editor->CanUndo() && ! editor->CanRedo() && notifications == 0u,
                  "editor adoption preserves source, revision, selection, history and notifications");
        }
        Check(observe([&] { Check(source.MatchesText(text), "exact source matches without flattening"); }) == 0u,
              "source comparison performs no C++ allocation");
        const auto shared = countedEditor->GetMarkdownSyntax();
        Check(observe([&] { Check(countedEditor->TryAdoptReadOnlyMarkdownSyntax(countedEditor->Snapshot(), owner) == S_OK, "repeat adoption succeeds"); }) ==
                      0u &&
                  countedEditor->GetMarkdownSyntax() == shared,
              "idempotent adoption performs no C++ allocation or publication replacement");
        Document history(L"x");
        Check(history.ReplaceSelection(L"y"), "seed a history ring before fault injection");
        const auto before = history.Snapshot();
        if (fatalScenario == 0u)
            Check(DxUiFailureTests::ExpectFatalChild(L"embedded-history-undo-allocation"), "first redo ring allocation terminates in its owned child");
        Check(history.Undo() && history.Text() == L"x" && history.Redo() && history.Snapshot().IsSameRevision(before),
              "undo and redo preserve the exact revision");
        bool cycled{};
        const auto warmHistoryAllocations = observe([&] { cycled = history.Undo() && history.Redo(); });
        Check(cycled && warmHistoryAllocations == 0u, "warm undo and redo reuse bounded history storage without allocating");
        if constexpr (requires(Editor& editor) {
                          editor.SetAutoPairEnabled(true);
                          editor.SetSmartTypographyEnabled(true);
                          editor.SetMarkdownAutoFormatEnabled(true);
                      })
        {
            enum class TypingPolicy
            {
                Pair,
                Typography,
                Fence
            };
            unsigned policyScenario = 2u;
            for (const auto policy : {TypingPolicy::Pair, TypingPolicy::Typography, TypingPolicy::Fence})
            {
                ++policyScenario;
                if (fatalScenario != 0u && fatalScenario != policyScenario)
                    continue;
                {
                    ControlHost host;
                    auto root    = std::make_unique<Editor>();
                    auto* editor = root.get();
                    if (policy == TypingPolicy::Typography)
                    {
                        editor->SetText(L"..");
                        editor->SetSelection({2u, 2u});
                        editor->SetSmartTypographyEnabled(true);
                    }
                    else if (policy == TypingPolicy::Pair)
                        editor->SetAutoPairEnabled(true);
                    else
                    {
                        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
                        editor->SetViewMode(ViewMode::Source);
                        editor->SetReadOnly(true);
                        Parser parser;
                        const auto policySource = editor->Snapshot();
                        parser.Begin(policySource, editor->Revision(), MarkdownProfile::RichMarkdown);
                        finish(parser);
                        Check(editor->TryAdoptReadOnlyMarkdownSyntax(policySource, parser.Published()) == S_OK, "prepare autoformat syntax without a window");
                        editor->SetReadOnly(false);
                        editor->SetViewMode(ViewMode::Rich);
                        editor->SetMarkdownRenderingEnabled(true);
                        editor->SetMarkdownAutoFormatEnabled(true);
                        Check(editor->GetMarkdownSyntax() != nullptr, "autoformat starts with a current published syntax");
                    }
                    host.SetRoot(std::move(root));
                    if (policy == TypingPolicy::Fence)
                        Check(editor->OnChar(host, L'`', 0u) && editor->OnChar(host, L'`', 0u), "seed a pending fenced-code trigger");
                    const auto act = [&]
                    {
                        return editor->OnChar(host, policy == TypingPolicy::Pair ? L'(' : policy == TypingPolicy::Typography ? L'.' : L'`', 0u);
                    };
                    if (fatalScenario != 0u)
                    {
                        RichSnapshotAdoptionFaultOrigin([&] { (void)act(); });
                        return;
                    }
                    constexpr const wchar_t* children[] = {
                        L"embedded-typing-pair-allocation", L"embedded-typing-typography-allocation", L"embedded-typing-fence-allocation"};
                    const auto expected = policy == TypingPolicy::Pair ? L"()" : policy == TypingPolicy::Typography ? L"\u2026" : L"```\r\n\r\n```";
                    const auto restored = policy == TypingPolicy::Typography ? L".." : L"";
                    Check(act() && editor->GetText() == expected && editor->Undo() && editor->GetText() == restored,
                          "typing-policy transaction commits and undoes atomically");
                    // Child startup and dump verification must not split a timed typing sequence.
                    Check(DxUiFailureTests::ExpectFatalChild(children[policyScenario - 3u]), "typing policy allocation terminates in its owned child");
                }
            }
        }
        std::cout << "parsed snapshot allocation schedule: parser=" << parserRequests << " editor=" << editorRequests << '\n';
    }
    else
        Check(false, "current ordinary qualification requires parsed snapshot reuse support");
}

template <typename Editor> __declspec(noinline) static void RichDefaultMenuFaultOrigin(Editor& editor, std::vector<DxUi::MenuFlyoutItem>& items)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    editor.BuildDefaultContextMenuItems(items);
}
template <typename Editor = DxUi::RichTextEditor> static void TestRichDefaultMenuModelAllocationFailureAtomicity(bool fatalChild = false)
{
    using namespace DxUi;
    static_assert(std::is_aggregate_v<MenuFlyoutItem>);
    if constexpr (! requires { typename Editor::DefaultContextMenuLabels; })
    {
        Check(false, "current qualification requires rich default-menu model support");
        return;
    }
    else
    {
        static_assert(! std::is_nothrow_default_constructible_v<MenuFlyoutItem> || ! std::is_same_v<Editor, Editor>);
        Editor editor(L"word");
        const typename Editor::DefaultContextMenuLabels labels{L"Undo", L"Redo", L"Cut", L"Copy", L"Paste", L"Select all", L"Bold", L"Italic"};
        editor.SetDefaultContextMenuLabels(&labels);
        std::vector<MenuFlyoutItem> items(0u);
        items.emplace_back();
        items.front().text = L"sentinel";
        if (fatalChild)
        {
            RichDefaultMenuFaultOrigin(editor, items);
            return;
        }
        Check(DxUiFailureTests::ExpectFatalChild(L"embedded-default-menu-allocation"), "default menu allocation terminates in its owned child");
        editor.BuildDefaultContextMenuItems(items);
        Check(items.size() == 8u, "default-menu model publishes all items");
    }
}

static void TestRichEditorSteadyLinkHoverAllocatesNothing(GraphicsFixture& gpu)
{
    using namespace DxUi;
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
    class HoverEditor final : public RichTextEditor
    {
    public:
        using RichTextEditor::GetTextInputRangeRects;
        using RichTextEditor::RichTextEditor;
    };
#pragma warning(pop)
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "link hover shared WARP resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "link hover embedded host");
    const std::wstring source = L"before [linked **label**](local) after";
    auto owned                = std::make_unique<HoverEditor>(source);
    auto* editor              = owned.get();
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    editor->SetLinkUnderlinePolicy(RichTextLinkUnderlinePolicy::HoverOrCaret);
    view.Controls().SetRoot(std::move(owned));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare strict link hit geometry");
    const auto first      = source.find(L"linked");
    const auto rectangles = editor->GetTextInputRangeRects(view.Controls(), first, first + 6u);
    Check(rectangles && ! rectangles->empty(), "link hover has visible prepared glyph fragments");
    const auto rectangle = rectangles->front();
    const auto point     = D2D1::Point2F(rectangle.left + 0.1f, (rectangle.top + rectangle.bottom) * 0.5f);
    static_cast<void>(editor->OnMouseMove(view.Controls(), point, 0u));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "warm link hover publication");
    const auto generation     = editor->DebugGetState().layoutGeneration;
    const auto oldCounting    = countAllocations;
    const auto oldAllocations = allocations, oldBytes = allocationBytes;
    size_t requests{}, bytes{};
    bool underlined = true;
    {
        const auto restore = wil::scope_exit([=]() noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldAllocations;
            allocationBytes  = oldBytes;
        });
        countAllocations   = true;
        allocations = allocationBytes = 0u;
        for (unsigned step = 0u; step < 1024u; ++step)
        {
            static_cast<void>(editor->OnMouseMove(view.Controls(), D2D1::Point2F(point.x + static_cast<float>(step % 2u) * .1f, point.y), 0u));
            underlined = underlined && editor->IsLinkUnderlineVisible(first);
        }
        requests = allocations;
        bytes    = allocationBytes;
    }
    Check(underlined && ! requests && ! bytes && editor->DebugGetState().layoutGeneration == generation,
          "1024 steady glyph hovers allocate no C++ storage, reflow no text and keep the native link cue");
}
template <typename Operation> __declspec(noinline) static void RichImageDecoderFaultOrigin(Operation&& operation)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)operation();
}
static void TestRichImageDecoderAllocationFailures(unsigned fatalScenario = 0u)
{
    using DxUi::RichText::RichImage;
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Check(SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE, "image failure fixture initializes or borrows COM");
    const auto apartment = wil::scope_exit([&] noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    wil::com_ptr<IWICImagingFactory> factory;
    Hr(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())), "image failure factory");
    std::array<BYTE, 24u> source{};
    for (unsigned i = 0u; i < 6u; ++i)
    {
        source[i * 4u]      = static_cast<BYTE>((i + 1u) * 30u);
        source[i * 4u + 3u] = 255u;
    }
    const auto encoded = RichImageTests::Encode(factory.get(), GUID_ContainerFormatPng, source, 2u, 3u);
    std::shared_ptr<const RichImage> image;
    const auto sweep = [&](unsigned scenario, auto&& request)
    {
        if (fatalScenario != 0u && fatalScenario != scenario)
            return;
        if (fatalScenario == scenario)
        {
            RichImageDecoderFaultOrigin(request);
            return;
        }
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const bool oldCounting = countAllocations;
        const auto restore     = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        const auto result             = request();
        countAllocations              = false;
        Hr(result, "baseline image allocation schedule");
        const auto requests = allocations;
        Check(requests > 0u && requests < 128u, "immutable pixel creation has a bounded observed allocation schedule");
        Check(DxUiFailureTests::ExpectFatalChild(scenario == 1u ? L"embedded-image-decode-allocation" : L"embedded-image-create-allocation"),
              "image allocation terminates in its owned child");
        Check(image->Width() == 2u && image->Height() == 3u && image->Pixels()[20u] == std::byte(180u), "image creation retains exact known pixels");
        std::cout << "immutable image allocation points=" << requests << '\n';
    };
    sweep(1u, [&] { return RichImage::Decode(factory.get(), std::as_bytes(std::span(encoded)), image); });
    sweep(2u, [&] { return RichImage::CreatePbgra(2u, 3u, std::as_bytes(std::span(source)), image); });
    if (fatalScenario != 0u)
        return;
    constexpr unsigned extent   = 512u;
    constexpr size_t pixelBytes = static_cast<size_t>(extent) * extent * 4u;
    std::vector<BYTE> opaque(pixelBytes, 0u);
    for (size_t i = 0u; i < pixelBytes; i += 4u)
    {
        opaque[i]      = 37u;
        opaque[i + 1u] = 83u;
        opaque[i + 2u] = 149u;
        opaque[i + 3u] = 255u;
    }
    const auto largePng = RichImageTests::Encode(factory.get(), GUID_ContainerFormatPng, opaque, extent, extent);
    std::shared_ptr<const RichImage> warm;
    Hr(RichImage::Decode(factory.get(), std::as_bytes(std::span(largePng)), warm), "warm identity PNG decoder");
    size_t bytes{};
    HRESULT decode{};
    {
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const bool oldCounting = countAllocations;
        const auto restore     = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        decode                        = RichImage::Decode(factory.get(), std::as_bytes(std::span(largePng)), image);
        countAllocations              = false;
        bytes                         = allocationBytes;
    }
    Hr(decode, "decode identity PNG under allocation counter");
    Check(bytes >= pixelBytes && bytes < pixelBytes + pixelBytes / 2u, "identity PNG decoding allocates one full owned C++ pixel buffer, not a second copy");
    Check(image && image->Width() == extent && image->Height() == extent && image->Pixels().size() == opaque.size() &&
              std::memcmp(image->Pixels().data(), opaque.data(), opaque.size()) == 0,
          "identity PNG preserves every opaque source pixel");
}
__declspec(noinline) static void RichImagePathFaultOrigin(unsigned scenario, std::span<const std::byte> bytes)
{
    DxUi::TextClipboardContent content;
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    if (scenario == 1u)
        (void)DxUi::DecodeClipboardFilePaths(bytes, content);
    else if (scenario == 2u)
        (void)DxUi::RichText::Detail::BuildMarkdownFileReference(L"C:\\notes\\chapters\\note.md", L"C:\\notes\\files\\\u6771\U0001f600 [x]#%.pdf");
    else if (scenario == 3u)
        (void)DxUi::RichText::Detail::ResolveLocalImagePath(L"C:\\notes\\chapters\\note.md", L"../images/%E6%9D%B1%F0%9F%98%80.png");
    else
        (void)DxUi::RichText::Detail::BuildMarkdownFileReference(L"C:\\notes\\chapters\\note.md", L"\\\\?\\C:\\notes\\images\\\u6771\U0001f600 [x]#%.png");
}
static void TestRichImagePathAllocationFailures(unsigned fatalScenario = 0u)
{
    using namespace DxUi;
    constexpr wchar_t list[] = L"C:\\images\\\u6771\U0001f600.png\0D:\\images\\other.jpg\0";
    const auto bytes         = RichImagePathTests::DropBuffer(std::wstring_view(list, std::size(list)));
    if (fatalScenario != 0u)
    {
        RichImagePathFaultOrigin(fatalScenario - 1u, bytes);
        return;
    }
    const auto image = RichText::Detail::BuildMarkdownFileReference(L"C:\\notes\\chapters\\note.md", L"\\\\?\\C:\\notes\\images\\\u6771\U0001f600 [x]#%.png");
    Check(image && *image == L"![\u6771\U0001f600 \\[x\\]\\#\\%](../images/%E6%9D%B1%F0%9F%98%80%20%5Bx%5D%23%25.png)",
          "image destination and escaped source are exact");
    TextClipboardContent content;
    Check(DecodeClipboardFilePaths(bytes, content) == S_OK && content.filePaths.size() == 2u && content.filePaths[0] == L"C:\\images\\\u6771\U0001f600.png",
          "file list decodes exact Unicode paths");
    const auto link = RichText::Detail::BuildMarkdownFileReference(L"C:\\notes\\chapters\\note.md", L"C:\\notes\\files\\\u6771\U0001f600 [x]#%.pdf");
    Check(link && *link == L"[\u6771\U0001f600 \\[x\\]\\#\\%\\.pdf](../files/%E6%9D%B1%F0%9F%98%80%20%5Bx%5D%23%25.pdf)",
          "file-link destination and full label are exact");
    const auto path = RichText::Detail::ResolveLocalImagePath(L"C:\\notes\\chapters\\note.md", L"../images/%E6%9D%B1%F0%9F%98%80.png");
    Check(path && *path == L"C:\\notes\\images\\\u6771\U0001f600.png", "URI/base resolution preserves exact Unicode paths");
    for (const auto child : {L"embedded-image-reference-allocation",
                             L"embedded-file-list-decode-allocation",
                             L"embedded-file-reference-allocation",
                             L"embedded-image-path-resolution-allocation"})
        Check(DxUiFailureTests::ExpectFatalChild(child), "image path allocation terminates in its owned child");
}

static void TestFootnoteBlockAllocationFailures(bool trace = false)
{
    using namespace DxUi::RichText;
    const auto finish = [](auto& state)
    {
        while (state.HasPendingPreparation())
            state.Advance(7u);
    };
    MarkdownParser baseline;
    baseline.Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
    finish(baseline);
    const auto oldSyntax = baseline.Published();
    std::size_t total{};
    const auto sweep = [&](auto make, auto request, auto sourceOf, std::wstring_view expected)
    {
        std::size_t requests{};
        {
            auto state             = make();
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
            request(*state);
            finish(*state);
            countAllocations = false;
            requests         = allocations;
        }
        Check(requests > 0u && requests < 2048u, "note preparation has a finite independently observed allocation schedule");
        auto state     = make();
        const auto old = state->Published();
        request(*state);
        Check(state->Published() == old && sourceOf(old) == L"previous", "note preparation retains its old publication while staging");
        finish(*state);
        Check(sourceOf(state->Published()) == expected && sourceOf(old) == L"previous", "note preparation publishes exact source and retains old snapshots");
        if (trace)
            std::fprintf(stderr, "note preparation measured %zu allocations; fatal allocation covered by isolated children\n", requests);
        total += requests;
    };
    for (const auto* source : {L"[^n]: first\r\n\r\n    second\r\n",
                               L"- [^n]:\n      > - nested\n      >\n      >   body\n",
                               L"[^\u03b2\U0001f600]: **body**\n[^m]: next\n",
                               L"main[^X] and[^missing]\n\n[^x]: body[^y]\n[^X]: duplicate\n[^y]: body[^x]\n",
                               L"> - [^n]: **note**\n\nmain <a href='#x'>[^n]</a> again[^n]\n"})
    {
        sweep(
            [&]
        {
            auto value = std::make_unique<MarkdownParser>();
            value->Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
            finish(*value);
            return value;
        },
            [&](auto& value) { value.Begin(source, 2u, MarkdownProfile::RichMarkdown); },
            [](const auto& value) { return value->Source(); },
            source);
        MarkdownParser parser;
        parser.Begin(source, 2u, MarkdownProfile::RichMarkdown);
        finish(parser);
        const auto syntax = parser.Published();
        sweep(
            [&]
        {
            auto value = std::make_unique<SemanticBuilder>();
            value->Begin(oldSyntax);
            finish(*value);
            return value;
        },
            [&](auto& value) { value.Begin(syntax); },
            [](const auto& value) { return value->Map().Source(); },
            source);
    }
    std::cout << "note block/binding parser/semantic scenarios=10 measuredRequests=" << total << " heldSourcePreserved=1\n";
}

static void TestScopedSemanticSourceAllocationFailures(bool trace = false)
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto oldSyntax      = RichSemanticDocumentTests::Parse(L"previous", MarkdownProfile::RichMarkdown);
    const auto pendingSyntax  = RichSemanticDocumentTests::Parse(L"pending", MarkdownProfile::RichMarkdown);
    const auto finishSemantic = [](auto& builder)
    {
        while (builder.HasPendingPreparation())
            builder.Advance(7u);
    };
    std::size_t semanticTotal{}, geometryTotal{}, parserTotal{};
    const auto sweep = [&](auto make, auto request, auto finish, auto pending, auto key, auto verify, auto clear)
    {
        auto measured = make();
        std::size_t requests{};
        {
            const auto oldCounting = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=]() noexcept
            {
                countAllocations = oldCounting;
                allocations      = oldCalls;
                allocationBytes  = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations              = true;
            request(*measured);
            finish(*measured);
            countAllocations = false;
            requests         = allocations;
        }
        Check(requests > 0u && requests < 4096u, "scoped source preparation has a finite independently observed allocation schedule");
        auto state     = make();
        const auto old = key(*state);
        request(*state);
        Check(key(*state) == old && pending(*state), "scoped source staging retains its previous publication");
        finish(*state);
        verify(*state);
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = 0u;
            clear(*state);
        }
        if (trace)
            std::fprintf(stderr, "scoped source preparation measured %zu allocations; fatal allocation uses isolated children\n", requests);
        return requests;
    };
    for (const auto source : {L"", L"~~broken\r\n[^n]: **&amp;**\r\n---\r\n", L"> ```\nmissing prefix\n> ```\n"})
    {
        const Document model(source);
        const auto finishParser = [](auto& parser)
        {
            while (parser.HasPendingPreparation())
                parser.Advance(1u);
        };
        parserTotal += sweep(
            [&]
        {
            auto parser = std::make_unique<MarkdownParser>();
            parser->Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
            finishParser(*parser);
            parser->Begin(L"pending", 2u, MarkdownProfile::RichMarkdown);
            return parser;
        },
            [&](auto& parser) { parser.BeginSourceRegion(model.Snapshot(), model.Revision(), {{0u, model.Length()}, 19u}); },
            finishParser,
            [](const auto& parser) { return parser.HasPendingPreparation(); },
            [](const auto& parser) { return std::pair{parser.Published().get(), std::uint64_t{}}; },
            [&](const auto& parser)
        {
            Check(parser.Published()->SourceRegion() == MarkdownSourceRegion{{0u, model.Length()}, 19u},
                  "opaque parser source retry restores the requested region, including empty EOF and malformed syntax");
        },
            [](auto& parser)
        {
            parser.Cancel();
            parser.Clear();
        });
    }
    for (const auto scenario : {0u, 2u, 4u, 6u, 8u, 13u})
    {
        const auto syntax = RichSemanticDocumentTests::Parse(SemanticSourceBlockTests::cases[scenario], MarkdownProfile::RichMarkdown);
        const auto node   = SemanticSourceBlockTests::SourceBlock(*syntax);
        semanticTotal += sweep(
            [&]
        {
            auto builder = std::make_unique<SemanticBuilder>();
            builder->Begin(oldSyntax);
            finishSemantic(*builder);
            builder->Begin(pendingSyntax);
            return builder;
        },
            [&](auto& builder) { builder.Begin(syntax, node); },
            finishSemantic,
            [](const auto& builder) { return builder.HasPendingPreparation(); },
            [](const auto& builder) { return std::pair{builder.Published().get(), std::uint64_t{}}; },
            [&](const auto& builder)
        {
            Check(builder.Published()->Syntax() == syntax && builder.Published()->SourceBlock() == node,
                  "explicit source-mode retry restores the requested exact scope");
        },
            [](auto& builder)
        {
            builder.Cancel();
            builder.Clear();
        });
    }
    const auto write          = PreparedFlowchartTests::WriteFactory();
    const auto drawing        = PreparedFlowchartTests::DrawingFactory();
    const auto oldDocument    = SemanticDiagramTests::Build(L"previous", Check);
    const auto finishGeometry = [](auto& geometry)
    {
        while (geometry.Pending())
            geometry.Advance({1u, 3u});
    };
    for (const auto scenario : {0u, 13u})
    {
        const auto syntax = RichSemanticDocumentTests::Parse(SemanticSourceBlockTests::cases[scenario], MarkdownProfile::RichMarkdown);
        const auto node   = SemanticSourceBlockTests::SourceBlock(*syntax);
        SemanticBuilder builder;
        builder.Begin(syntax, node);
        finishSemantic(builder);
        const auto document = builder.Published();
        LayoutOptions options;
        options.widthDip = 1024.f;
        geometryTotal += sweep(
            [&]
        {
            auto geometry       = std::make_unique<PreparedSemanticGeometry>();
            auto oldOptions     = options;
            oldOptions.widthDip = 800.f;
            geometry->Begin(write.get(), oldDocument, oldOptions, drawing.get(), {1u, 1u});
            finishGeometry(*geometry);
            oldOptions.widthDip = 900.f;
            geometry->Begin(write.get(), oldDocument, oldOptions, drawing.get(), {2u, 2u});
            return geometry;
        },
            [&](auto& geometry) { geometry.Begin(write.get(), document, options, drawing.get(), {3u, 3u}); },
            finishGeometry,
            [](const auto& geometry) { return geometry.Pending(); },
            [](const auto& geometry) { return std::pair{&geometry.Document(), geometry.Generation()}; },
            [&](const auto& geometry)
        {
            Check(geometry.Ready() && &geometry.Document() == document.get() && geometry.HasNativeObjects() == (scenario == 13u),
                  "source-mode geometry retries with the same exact raw scope and unaffected neighboring native graph");
        },
            [](auto& geometry)
        {
            geometry.Cancel();
            geometry.Invalidate();
            geometry.Clear();
        });
    }
    std::cout << "scoped source parserScenarios=3 measuredRequests=" << parserTotal << " semanticScenarios=6 measuredRequests=" << semanticTotal
              << " geometryScenarios=2 measuredRequests=" << geometryTotal << " preservedPublication=1 retirementAllocations=0\n";
}

static void TestSemanticDiagramAllocationFailures(bool trace = false)
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto oldSyntax      = RichSemanticDocumentTests::Parse(L"previous", MarkdownProfile::RichMarkdown);
    const auto pendingSyntax  = RichSemanticDocumentTests::Parse(L"pending", MarkdownProfile::RichMarkdown);
    const auto finishSemantic = [](SemanticBuilder& builder)
    {
        while (builder.HasPendingPreparation())
            builder.Advance(7u);
    };
    // Count the actual allocation schedule, including native parsing, captions,
    // normalized-to-raw provenance, diagnostic publication and footer ownership.
    const std::array<std::wstring_view, 20u> sources = {SemanticDiagramTests::scenarios[5u].source,
                                                        L"main[^n]\r\n\r\n[^n]: note\r\n    > ```mermaid\r\n    > graph LR;A{wide}-->|yes|B\r\n    > ```\r\n",
                                                        SemanticDiagramTests::scenarios[11u].source,
                                                        SemanticDiagramTests::scenarios[8u].source,
                                                        L"```mermaid\nsequenceDiagram\nparticipant A\nparticipant B\nA->>B: request\nB-->>A: reply\n```\n",
                                                        L"```mermaid\nsequenceDiagram\nactor Writer\nparticipant App\nautonumber\n"
                                                        L"Writer->>App: Send\nNote over Writer,App: Review\nApp-->>Writer: Ready\n```\n",
                                                        L"```mermaid\npie\n\"Dogs\": 3\n\"Cats\": 2\n```\n",
                                                        L"```mermaid\nerDiagram\nCUSTOMER ||--o{ ORDER : places\nORDER ||--|{ ITEM : contains\n```\n",
                                                        L"```mermaid\ngantt\nCompile : 2024-02-27, 3d\nTest : 2024-03-01, 2d\n```\n",
                                                        L"```mermaid\nmindmap\n root\n  First\n   Leaf\n  Second\n```\n",
                                                        L"```math\n\\frac{1}{2}\n```\n",
                                                        L"$$\n\\frac{1}{2}\n$$\n",
                                                        L"```latex\n\\frac{1}{2}\n",
                                                        L"<p>x<sup><b>hello</b></sup> y<sub>world</sub></p>\n",
                                                        L"x<sup>![alt](image.png)</sup>\n",
                                                        L"<details open>\n<summary><b>More</b></summary>\nBody text\n</details>\n",
                                                        L"> [!NOTE]\n> Body **bold**\n",
                                                        L"> [!tip]- Closed title\n> Hidden **body**\n",
                                                        L"Term\n: first description\n: second description\n",
                                                        L"HTML document\n\n*[HTML]: Hyper Text Markup Language\n"};
    const auto observe                               = [](auto&& operation)
    {
        const auto oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const auto restore = wil::scope_exit([=]() noexcept
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
    std::size_t semanticRequests{}, geometryRequests{};
    for (const auto source : sources)
    {
        const auto syntax = [&]
        {
            if (! source.starts_with(L"Term\n:") && ! source.starts_with(L"HTML document"))
                return RichSemanticDocumentTests::Parse(source, MarkdownProfile::RichMarkdown);
            MarkdownParser parser;
            parser.Begin(source,
                         1u,
                         MarkdownProfile::RichMarkdown,
                         MarkdownExtensions::RichAll | MarkdownExtensions::DefinitionLists | MarkdownExtensions::Abbreviations);
            while (parser.HasPendingPreparation())
                parser.Advance(7u);
            return parser.Published();
        }();
        const auto begin = [&](SemanticBuilder& builder)
        {
            if (source.starts_with(L"<details"))
            {
                const std::array states{DxUi::RichText::Detail::DisclosureOverride{syntax->Nodes()[1u].identity, false}};
                DxUi::RichText::Detail::SemanticExtensionAccess::BeginWithPresentation(builder, syntax, {}, states);
            }
            else
                builder.Begin(syntax);
        };
        SemanticBuilder measured;
        measured.Begin(oldSyntax);
        finishSemantic(measured);
        measured.Begin(pendingSyntax);
        const auto requests = observe([&]
        {
            begin(measured);
            finishSemantic(measured);
        });
        Check(requests > 0u && requests < 4096u, "native semantic preparation has a finite observed allocation schedule");
        SemanticBuilder builder;
        builder.Begin(oldSyntax);
        finishSemantic(builder);
        const auto old = builder.Published();
        builder.Begin(pendingSyntax);
        begin(builder);
        Check(builder.Published() == old && old->Map().Source() == L"previous", "native semantic staging retains the previous publication");
        finishSemantic(builder);
        Check(builder.Published()->Map().Source() == source && old->Map().Source() == L"previous", "native semantic source publishes exactly");
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = 0u;
            builder.Cancel();
            builder.Clear();
        }
        if (trace)
            std::fprintf(stderr, "semantic diagram measured %zu allocations; fatal allocation uses isolated children\n", requests);
        semanticRequests += requests;
    }
    const auto write                            = PreparedFlowchartTests::WriteFactory();
    const auto drawing                          = PreparedFlowchartTests::DrawingFactory();
    const auto oldDocument                      = SemanticDiagramTests::Build(L"previous", Check);
    constexpr std::size_t geometryScenarioCount = 5u;
    for (std::size_t scenario = 0u; scenario < geometryScenarioCount; ++scenario)
    {
        const auto document = SemanticDiagramTests::Build(
            scenario == 4u ? L"<p>x<sup>hello</sup> y<sub>world</sub></p>\n" : SemanticDiagramTests::scenarios[scenario == 3u ? 0u : scenario].source, Check);
        LayoutOptions options;
        options.widthDip = scenario == 3u ? 40.0f : 1024.0f;
        LayoutOptions previousOptions;
        previousOptions.widthDip = 800.0f;
        const auto finish        = [](PreparedSemanticGeometry& geometry)
        {
            while (geometry.Pending())
                if (geometry.Advance({1u, 3u}).status == LayoutPreparationStatus::Failed)
                    return false;
            return geometry.RejectedNativeObjects().empty();
        };
        PreparedSemanticGeometry measured;
        measured.Begin(write.get(), oldDocument, previousOptions, drawing.get(), {1u, 1u});
        finish(measured);
        auto pendingOptions     = previousOptions;
        pendingOptions.widthDip = 900.0f;
        measured.Begin(write.get(), oldDocument, pendingOptions, drawing.get(), {2u, 2u});
        const auto requests = observe([&]
        {
            measured.Begin(write.get(), document, options, drawing.get(), {3u, 3u});
            finish(measured);
        });
        Check(requests > 0u && requests < 1024u, "native semantic geometry has a finite observed allocation schedule");
        PreparedSemanticGeometry geometry;
        geometry.Begin(write.get(), oldDocument, previousOptions, drawing.get(), {1u, 1u});
        finish(geometry);
        const auto generation = geometry.Generation();
        geometry.Begin(write.get(), document, options, drawing.get(), {3u, 3u});
        Check(geometry.Ready() && &geometry.Document() == oldDocument.get() && geometry.Generation() == generation,
              "new geometry staging retains its previous publication");
        const auto ready = finish(geometry);
        Check(scenario == 3u ? ! ready && &geometry.Document() == oldDocument.get()
                             : ready && (scenario == 4u || geometry.HasNativeObjects()) && &geometry.Document() == document.get(),
              "geometry preparation distinguishes intrinsic-width capacity from successful publication");
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=]() noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = 0u;
            geometry.Cancel();
            geometry.Invalidate();
            geometry.Clear();
        }
        geometryRequests += requests;
    }
    std::cout << "semantic diagram/math/HTML scenarios=" << std::size(sources) << " measuredRequests=" << semanticRequests
              << " geometryScenarios=" << geometryScenarioCount << " measuredRequests=" << geometryRequests
              << " preservedPublication=1 retirementAllocations=0\n";
}

template <typename Operation> __declspec(noinline) static void RichPreparedFlowchartFaultOrigin(Operation&& operation)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    operation();
}
static void TestPreparedFlowchartAllocationFailures(bool trace = false, unsigned fatalScenario = 0u)
{
    static_cast<void>(trace);
    using namespace PreparedFlowchartTests;
    const auto write    = WriteFactory();
    const auto drawing  = DrawingFactory();
    const auto oldGraph = FlowchartLayoutTests::Graph(L"graph TD;Old");
    const auto rich     = FlowchartLayoutTests::Graph(L"graph LR;A{\"Unicode \u03b2 \U0001f600\"}-->|yes|B((second));B-->A;A-->A");
    const auto huge     = FlowchartLayoutTests::Graph(L"graph TD;A[\"" + std::wstring(1024u, L'W') + L"\"]");
    std::size_t total{};
    for (const bool capacity : {false, true})
    {
        const unsigned scenario = capacity ? 2u : 1u;
        if (fatalScenario != 0u && fatalScenario != scenario)
            continue;
        const auto graph = capacity ? huge : rich;
        LayoutOptions options;
        if (capacity)
        {
            options.fontSizeDip          = 512.0f;
            options.diagramLabelWidthDip = 1.0f;
        }
        std::size_t requests{};
        {
            PreparedFlowchart prepared;
            const auto oldCounting = countAllocations;
            const auto oldCalls = allocations, oldBytes = allocationBytes;
            const auto restore = wil::scope_exit([=]() noexcept
            {
                countAllocations = oldCounting;
                allocations      = oldCalls;
                allocationBytes  = oldBytes;
            });
            allocations = allocationBytes = 0u;
            countAllocations              = true;
            prepared.Begin(write.get(), drawing.get(), graph, options, {2u, 2u});
            Complete(prepared, Check);
            countAllocations = false;
            requests         = allocations;
            Check(capacity ? ! prepared.Ready() && prepared.Geometry()->nodes.empty() : prepared.Ready(),
                  "capacity and prepared scenarios start from independently observed outcomes");
            Check(requests > 0u && requests < 128u, "native diagram has a bounded observed C++ preparation allocation schedule");
        }
        PreparedFlowchart prepared;
        LayoutOptions oldOptions;
        prepared.Begin(write.get(), drawing.get(), oldGraph, oldOptions, {1u, 1u});
        Complete(prepared, Check);
        const auto old = prepared.Geometry();
        if (fatalScenario != 0u)
        {
            RichPreparedFlowchartFaultOrigin([&]
            {
                prepared.Begin(write.get(), drawing.get(), graph, options, {2u, 2u});
                Complete(prepared, Check);
            });
            return;
        }
        Check(DxUiFailureTests::ExpectFatalChild(capacity ? L"embedded-flowchart-capacity-allocation" : L"embedded-flowchart-prepared-allocation"),
              "flowchart preparation allocation terminates in its owned child");
        prepared.Begin(write.get(), drawing.get(), graph, options, {2u, 2u});
        Check(prepared.Geometry() == old, "flowchart staging preserves its published geometry");
        Complete(prepared, Check);
        Check(capacity ? ! prepared.Ready() && prepared.Geometry()->nodes.empty() : prepared.Ready(),
              "flowchart preparation reaches its prepared or capacity outcome");
        {
            const auto oldFailure = failAllocationAfter;
            const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
            failAllocationAfter   = 0u;
            prepared.Cancel();
            prepared.Invalidate();
            prepared.Clear();
        }
        Check(! prepared.Ready() && ! prepared.Pending(), "native resource retirement allocates no C++ storage");
        total += requests;
    }
    unsigned familyScenario = 2u;
    for (const auto source : {L"sequenceDiagram\nparticipant Alice\nparticipant Bob\nAlice->>Bob: request\nBob-->>Alice: reply\n",
                              L"pie\n\"Dogs\": 3\n\"Cats\": 2\n\"Birds\": 1\n",
                              L"erDiagram\nCUSTOMER ||--o{ ORDER : places\nORDER ||--|{ ITEM : contains\n",
                              L"gantt\ntitle Release\nCompile : 2024-02-27, 3d\nTest : 2024-03-01, 2d\n",
                              L"mindmap\n root\n  First\n   Leaf\n  Second\n",
                              L"flowchart LR\nA[/input/] --> B[(store)] --> C(((finish)))\n",
                              L"flowchart TD\nA[\"<b>Bold</b> <i>It</i>\"]-->|<u>go</u>|B\n",
                              L"flowchart TD\nclassDef default font-weight:bold\nA-->B\nlinkStyle 0 stroke-width:2px,stroke-dasharray:5 5\n"})
    {
        ++familyScenario;
        if (fatalScenario != 0u && fatalScenario != familyScenario)
            continue;
        const auto graph = FlowchartLayoutTests::Graph(source);
        LayoutOptions options;
        std::size_t requests{};
        {
            PreparedFlowchart prepared;
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
            prepared.Begin(write.get(), drawing.get(), graph, options, {2u, 2u});
            Complete(prepared, Check);
            countAllocations = false;
            requests         = allocations;
            Check(prepared.Ready(), "native family preparation succeeds before allocation sweep");
        }
        PreparedFlowchart prepared;
        if (fatalScenario != 0u)
        {
            RichPreparedFlowchartFaultOrigin([&]
            {
                prepared.Begin(write.get(), drawing.get(), graph, options, {2u, 2u});
                Complete(prepared, Check);
            });
            return;
        }
        constexpr const wchar_t* children[] = {L"embedded-flowchart-sequence-allocation",
                                               L"embedded-flowchart-pie-allocation",
                                               L"embedded-flowchart-er-allocation",
                                               L"embedded-flowchart-gantt-allocation",
                                               L"embedded-flowchart-mindmap-allocation",
                                               L"embedded-flowchart-shapes-allocation",
                                               L"embedded-flowchart-label-allocation",
                                               L"embedded-flowchart-style-allocation"};
        Check(DxUiFailureTests::ExpectFatalChild(children[familyScenario - 3u]), "diagram family allocation terminates in its owned child");
        prepared.Begin(write.get(), drawing.get(), graph, options, {2u, 2u});
        Complete(prepared, Check);
        Check(prepared.Ready(), "native diagram family preparation succeeds");
        const auto oldFailure = failAllocationAfter;
        failAllocationAfter   = 0u;
        prepared.Cancel();
        prepared.Invalidate();
        prepared.Clear();
        failAllocationAfter = oldFailure;
        total += requests;
    }
    if (fatalScenario == 0u || fatalScenario == 11u)
    {
        const auto graph = FlowchartLayoutTests::Graph(L"flowchart TD\nA-->B\nstyle A fill:#000,stroke:#ff0,color:#fff\nlinkStyle 0 stroke:#f00\n");
        wil::com_ptr<IWICImagingFactory> imaging;
        THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(200u, 200u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), D2D1::RenderTargetProperties(), target.put()));
        std::size_t requests{};
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
            DiagramPalette palette(graph, target.get());
            while (! palette.Advance())
            {
            }
            countAllocations = false;
            requests         = allocations;
        }
        if (fatalScenario != 0u)
        {
            RichPreparedFlowchartFaultOrigin([&]
            {
                DiagramPalette palette(graph, target.get());
                while (! palette.Advance())
                {
                }
            });
            return;
        }
        Check(DxUiFailureTests::ExpectFatalChild(L"embedded-diagram-palette-allocation"), "diagram palette allocation terminates in its owned child");
        DiagramPalette palette(graph, target.get());
        while (! palette.Advance())
        {
        }
        Check(palette.Ready(), "diagram palette staging succeeds");
        total += requests;
    }
    if (fatalScenario != 0u)
        return;
    static thread_local bool drawOldCounting{}, drawAllocated{};
    static thread_local std::size_t drawOldCalls{};
    drawAllocated = false;
    BorrowedTargets(Check,
                    [](bool before) noexcept
    {
        if (before)
        {
            drawOldCounting  = countAllocations;
            drawOldCalls     = allocations;
            countAllocations = true;
        }
        else
        {
            drawAllocated    = drawAllocated || allocations != drawOldCalls;
            countAllocations = drawOldCounting;
            allocations      = drawOldCalls;
        }
    });
    Check(! drawAllocated, "actual borrowed-target native drawing creates zero tracked C++ allocations");
    PaletteRaster(Check,
                  [](bool before) noexcept
    {
        if (before)
        {
            drawOldCounting  = countAllocations;
            drawOldCalls     = allocations;
            countAllocations = true;
        }
        else
        {
            drawAllocated    = drawAllocated || allocations != drawOldCalls;
            countAllocations = drawOldCounting;
            allocations      = drawOldCalls;
        }
    });
    Check(! drawAllocated, "palette drawing creates zero tracked C++ allocations");
    std::cout << "native paint scenarios=10 measuredRequests=" << total << " retirementAllocations=0 drawAllocations=0\n";
}
template <typename Operation> __declspec(noinline) static void RichNativeFlowchartLayoutFaultOrigin(Operation&& operation)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    operation();
}
static void TestNativeFlowchartLayoutAllocationFailureAtomicity(bool trace = false, unsigned fatalScenario = 0u)
{
    using namespace FlowchartLayoutTests;
    static_cast<void>(trace);
    std::size_t total{};
    for (const auto mode : {0u, 1u, 2u})
    {
        if (fatalScenario != 0u && fatalScenario != mode + 1u)
            continue;
        const auto graph = Graph(L"graph LR;A[\"Unicode \u03b2 \U0001f600\"]-->|yes|B((second));B-->A;A-->A");
        std::vector<FlowchartSize> nodes(graph->nodes.size(), {20.0, 12.0}), edges(graph->edges.size(), {20.0, 12.0});
        if (mode == 1u)
            nodes[0u].width = NativeFlowchartLayout::MaximumExtent;
        if (mode == 2u)
            for (auto& size : edges)
                size = {65500.0, 65500.0};
        std::size_t requests{};
        {
            NativeFlowchartLayout layout;
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
            layout.Begin(graph, nodes, edges);
            countAllocations = false;
            requests         = allocations;
            allocations      = 0u;
            countAllocations = true;
            Complete(layout, Check, 7u);
            Check(allocations == 0u, "graph geometry Advance allocates nothing after bounded admission");
            countAllocations = false;
        }
        Check(requests > 0u && requests < 32u, "graph geometry has a bounded observed admission allocation schedule");
        NativeFlowchartLayout layout;
        const auto oldGraph                           = Graph(L"graph TD;Old");
        const std::array<FlowchartSize, 1u> oldLabels = {{{16.0, 12.0}}};
        layout.Begin(oldGraph, oldLabels, {});
        const auto old = Complete(layout, Check);
        if (fatalScenario != 0u)
        {
            RichNativeFlowchartLayoutFaultOrigin([&] { layout.Begin(graph, nodes, edges); });
            return;
        }
        constexpr const wchar_t* children[] = {
            L"embedded-flowchart-layout-normal", L"embedded-flowchart-layout-node-capacity", L"embedded-flowchart-layout-edge-capacity"};
        Check(DxUiFailureTests::ExpectFatalChild(children[mode]), "graph layout admission allocation terminates in its owned child");
        layout.Begin(graph, nodes, edges);
        Check(layout.Published() == old, "layout staging retains previous publication");
        const auto result = Complete(layout, Check, 7u);
        Check(mode == 0u ? result->issue == FlowchartLayoutIssue::None && result->nodes.size() == 2u
                         : result->issue != FlowchartLayoutIssue::None && result->nodes.empty(),
              "graph layout reaches the intended prepared or capacity result");
        total += requests;
    }
    unsigned familyScenario = 3u;
    for (const auto source : {L"sequenceDiagram\nparticipant Alice\nparticipant Bob\nAlice->>Bob: request\nBob-->>Alice: reply\n",
                              L"pie\n\"Dogs\": 3\n\"Cats\": 2\n\"Birds\": 1\n",
                              L"erDiagram\nCUSTOMER ||--o{ ORDER : places\nORDER ||--|{ ITEM : contains\n",
                              L"gantt\ntitle Release\nCompile : 2024-02-27, 3d\nTest : 2024-03-01, 2d\n",
                              L"mindmap\n root\n  First\n   Leaf\n  Second\n"})
    {
        ++familyScenario;
        if (fatalScenario != 0u && fatalScenario != familyScenario)
            continue;
        const auto graph = Graph(source);
        const std::vector<FlowchartSize> nodes(graph->nodes.size(), {16.0, 12.0}), edges(graph->edges.size(), {16.0, 12.0});
        const std::vector<FlowchartSize> endpoints(graph->family == NativeDiagramFamily::EntityRelationship ? graph->edges.size() : 0u, {16.0, 12.0});
        const std::vector<FlowchartSize> groups(
            graph->family == NativeDiagramFamily::Pie || graph->family == NativeDiagramFamily::Gantt ? 1u : graph->groups.size(), {16.0, 12.0});
        std::size_t requests{};
        {
            NativeFlowchartLayout layout;
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
            layout.Begin(graph, nodes, edges, endpoints, endpoints, groups);
            countAllocations    = false;
            requests            = allocations;
            failAllocationAfter = 0u;
            const auto result   = Complete(layout, Check, 7u);
            failAllocationAfter = std::numeric_limits<size_t>::max();
            Check(result->issue == FlowchartLayoutIssue::None, "family layout advances without allocating after admission");
        }
        NativeFlowchartLayout layout;
        if (fatalScenario != 0u)
        {
            RichNativeFlowchartLayoutFaultOrigin([&] { layout.Begin(graph, nodes, edges, endpoints, endpoints, groups); });
            return;
        }
        constexpr const wchar_t* children[] = {L"embedded-flowchart-layout-sequence",
                                               L"embedded-flowchart-layout-pie",
                                               L"embedded-flowchart-layout-er",
                                               L"embedded-flowchart-layout-gantt",
                                               L"embedded-flowchart-layout-mindmap"};
        Check(DxUiFailureTests::ExpectFatalChild(children[familyScenario - 4u]), "diagram family layout allocation terminates in its owned child");
        layout.Begin(graph, nodes, edges, endpoints, endpoints, groups);
        Check(Complete(layout, Check, 7u)->issue == FlowchartLayoutIssue::None, "diagram family layout succeeds");
        total += requests;
    }
    std::cout << "native graph layout allocation scenarios=8 measuredRequests=" << total << " AdvanceAllocations=0 preservedPublication=1\n";
}
template <typename Operation> __declspec(noinline) static void RichNativeFlowchartParserFaultOrigin(Operation&& operation)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    operation();
}
static void TestNativeFlowchartAllocationFailureAtomicity(bool trace = false, unsigned fatalScenario = 0u)
{
    using namespace NativeFlowchartTests;
    static_cast<void>(trace);
    const std::wstring sources[] = {
        L"graph LR;A[\"Unicode \u03b2 \U0001f600\"]-->|yes|B((Second));B-->A;A-->A",
        L"graph TD;A[staged];B[another];A-->B;style A fill:red",
        L"graph TD;A[staged];B[another];A-->|\"incomplete label",
        L"graph TD;A[\"" + std::wstring(NativeFlowchartParser::MaximumLabelUnits + 1u, L'x') + L"\"]",
        L"flowchart LR;A & B-->|group|C & D-->E",
        L"flowchart LR;A-.->B;B==>C;C~~~D",
        L"stateDiagram-v2;state \"Ready\" as S;S : Running;S-->T : go",
        L"stateDiagram-v2;state C <<choice>>;A-->C;C-->B : yes",
        L"stateDiagram-v2;state F <<fork>>;state J <<join>>;A-->F;F-->J;J-->B",
        L"classDiagram;Base <|-- Derived;Derived : member",
        L"classDiagram\nclass Animal {\n+name: String\n+run()\n}\nAnimal -- Pond\n",
        L"sequenceDiagram\nparticipant Alice as Client\nparticipant Bob\nBob->>Alice: request\nAlice-->>Bob: response\nAlice-)Alice: retry\n",
        L"pie showData\ntitle Pets\n\"Dogs\": 3\n\"Cats\": 2\n\"Birds\": 1\n",
        L"erDiagram\nCUSTOMER ||--o{ ORDER : places\nCUSTOMER {\n string name\n int age\n}\nORDER ||--|{ ITEM : contains\n",
        L"gantt\ntitle Release\nsection Build\nCompile : 2024-02-27, 3d\nTest : 2024-03-01, 2d\n",
        L"mindmap\n  root((Central))\n    First\n      Leaf\n    Second\n",
        L"flowchart TD\nA:::heavy --> B\nclassDef heavy stroke-width:3px\n",
        L"flowchart TD\nA[\"<b>Bold</b> <i>It</i>\"]-->|<u>go</u>|B\n",
        L"flowchart TD\nclassDef default font-weight:bold\nA-->B\nlinkStyle 0 stroke-width:2px,stroke-dasharray:5 5\n",
        L"classDiagram\nnote \"a long standalone note\"\nclass A {\n+first: int\n+second: string\n}\n",
        L"mindmap\n root\n  child\n ::icon(bad)\n",
        L"pie\n\"one\": 1\n\"one\": 2\n",
        L"sequenceDiagram\nAlice->>Bob: hello\nAlice->>Missing: bad\n"};
    std::size_t total{};
    unsigned scenario = 0u;
    for (const auto& source : sources)
    {
        ++scenario;
        if (fatalScenario != 0u && fatalScenario != scenario)
            continue;
        std::size_t requests{};
        {
            NativeFlowchartParser parser;
            parser.Begin(L"graph TD;Old", 1u);
            Complete(parser);
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
            Complete(parser, 7u);
            countAllocations = false;
            requests         = allocations;
        }
        Check(requests > 0u && requests < 512u, "native flowchart has a bounded observed allocation schedule");
        NativeFlowchartParser parser;
        parser.Begin(L"graph TD;Old", 1u);
        const auto old = Complete(parser);
        if (fatalScenario != 0u)
        {
            RichNativeFlowchartParserFaultOrigin([&]
            {
                parser.Begin(source, 2u);
                Complete(parser, 7u);
            });
            return;
        }
        constexpr const wchar_t* children[] = {
            L"embedded-flowchart-parser-01", L"embedded-flowchart-parser-02", L"embedded-flowchart-parser-03", L"embedded-flowchart-parser-04",
            L"embedded-flowchart-parser-05", L"embedded-flowchart-parser-06", L"embedded-flowchart-parser-07", L"embedded-flowchart-parser-08",
            L"embedded-flowchart-parser-09", L"embedded-flowchart-parser-10", L"embedded-flowchart-parser-11", L"embedded-flowchart-parser-12",
            L"embedded-flowchart-parser-13", L"embedded-flowchart-parser-14", L"embedded-flowchart-parser-15", L"embedded-flowchart-parser-16",
            L"embedded-flowchart-parser-17", L"embedded-flowchart-parser-18", L"embedded-flowchart-parser-19", L"embedded-flowchart-parser-20",
            L"embedded-flowchart-parser-21", L"embedded-flowchart-parser-22", L"embedded-flowchart-parser-23"};
        Check(DxUiFailureTests::ExpectFatalChild(children[scenario - 1u]), "native parser allocation terminates in its owned child");
        parser.Begin(source, 2u);
        Check(parser.Published() == old, "parser staging retains the prior publication");
        Check(Complete(parser)->source == source, "native parser publishes the requested source");
        {
            NativeFlowchartParser cancellationParser;
            cancellationParser.Begin(source, 3u);
            cancellationParser.Advance(10u);
            const auto previous = failAllocationAfter;
            failAllocationAfter = 0u;
            cancellationParser.Cancel();
            failAllocationAfter = previous;
            Check(! cancellationParser.HasPendingPreparation(), "native family parser cancellation does not allocate");
        }
        total += requests;
    }
    std::cout << "native flowchart allocation scenarios=" << std::size(sources) << " measuredRequests=" << total << " sourcePreserved=1\n";
}

static void TestRichFrontMatterAllocationFailureAtomicity(bool trace = false)
{
    using namespace DxUi::RichText;
    const auto finish = [](MarkdownParser& parser)
    {
        while (parser.HasPendingPreparation())
            parser.Advance(7u);
    };
    std::size_t total         = 0u;
    std::size_t semanticTotal = 0u;
    const auto finishSemantic = [](SemanticBuilder& builder)
    {
        while (builder.HasPendingPreparation())
            builder.Advance(7u);
    };
    MarkdownParser oldParser;
    oldParser.Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
    finish(oldParser);
    const auto oldSyntax = oldParser.Published();
    std::size_t activationRequests{};
    {
        DxUi::RichTextEditor editor(L"previous");
        editor.SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
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
        editor.SetMarkdownRenderingEnabled(true);
        countAllocations   = false;
        activationRequests = allocations;
    }
    Check(activationRequests > 0u && activationRequests < 32u, "semantic activation lazily creates a bounded liveness state");
    DxUi::RichTextEditor editor(L"previous");
    editor.SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor.SetMarkdownRenderingEnabled(true);
    Check(editor.IsMarkdownRenderingEnabled() && editor.GetText() == L"previous" && ! editor.CanUndo(),
          "semantic activation preserves exact source and history");
    for (const std::wstring_view source : {L"---\r\n# comment\r\nname: '\U0001f600'\r\n---\r\n# after\r\n",
                                           L"---\rtitle: a\rtags: b\r---\r",
                                           L"+++\n# comment\nname = '\u03b2'\n+++\n",
                                           L"---\nkey: [\n",
                                           L"+++\nkey = [\n",
                                           L"+++\nx = 1\nx = 2\n+++\n",
                                           L"---\na: 1\na: 2\n---\n",
                                           L"+++\nnested = [{ x = [1, 2], text = \"long value\" }]\n+++\n",
                                           L"---\na: &anchor [one, two]\nb: *anchor\n? {x: 1, y: 2}\n: value\n---\n"})
    {
        std::size_t requests{};
        {
            MarkdownParser parser;
            parser.Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
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
            parser.Begin(source, 2u, MarkdownProfile::RichMarkdown);
            finish(parser);
            countAllocations = false;
            requests         = allocations;
        }
        Check(requests > 0u && requests < 512u, "metadata parser exposes a finite observed allocation schedule");
        MarkdownParser parser;
        parser.Begin(L"previous", 1u, MarkdownProfile::RichMarkdown);
        finish(parser);
        const auto old = parser.Published();
        parser.Begin(source, 2u, MarkdownProfile::RichMarkdown);
        Check(parser.Published() == old, "metadata staging retains the old publication");
        finish(parser);
        Check(parser.Published()->Source() == source && old->Source() == L"previous" &&
                  parser.Published()->Nodes()[1u].block.kind == MarkdownBlockKind::FrontMatter,
              "metadata preparation retains exact source and held publication");
        if (trace)
            std::fprintf(stderr, "front matter measured %zu allocations; fatal allocation uses isolated children\n", requests);
        total += requests;
        MarkdownParser parsed;
        parsed.Begin(source, 2u, MarkdownProfile::RichMarkdown);
        finish(parsed);
        const auto syntax          = parsed.Published();
        const auto beginProperties = [&](SemanticBuilder& builder)
        {
            if (syntax->Nodes()[1u].block.closed && syntax->Diagnostics().empty())
            {
                const std::array states{DxUi::RichText::Detail::DisclosureOverride{syntax->Nodes()[1u].identity, true}};
                DxUi::RichText::Detail::SemanticExtensionAccess::BeginWithPresentation(builder, syntax, {}, states);
            }
            else
                builder.Begin(syntax);
        };
        std::size_t semanticRequests{};
        {
            SemanticBuilder builder;
            builder.Begin(oldSyntax);
            finishSemantic(builder);
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
            beginProperties(builder);
            finishSemantic(builder);
            countAllocations = false;
            semanticRequests = allocations;
        }
        Check(semanticRequests > 0u && semanticRequests < 512u, "metadata semantic publication has a finite independently observed allocation schedule");
        SemanticBuilder builder;
        builder.Begin(oldSyntax);
        finishSemantic(builder);
        const auto oldSemantic = builder.Published();
        beginProperties(builder);
        Check(builder.Published() == oldSemantic, "metadata semantic staging retains the old publication");
        finishSemantic(builder);
        Check(builder.Published()->Syntax() == syntax && builder.Published()->Map().Source() == source && oldSemantic->Map().Source() == L"previous",
              "semantic metadata and missing-fence help preserve source and held publication");
        semanticTotal += semanticRequests;
    }
    std::cout << "front matter parser measuredRequests=" << total << " semanticMeasuredRequests=" << semanticTotal
              << " activationMeasuredRequests=" << activationRequests << " sourcePreserved=1\n";
}

static void TestRichMarkdownParserFailureAndStorageBudgets(bool trace = false)
{
    using namespace DxUi::RichText;
    const std::wstring source =
        L"# staged\r\n> - **nested *styles***\n>\n>   [a\nb]: /reference 'long title'\n\n"
        L"| header | second | third |\n| --- | --- | --- |\n| `a\\|b` |\n\n- [x] ~~[a b]~~\n\nwww.example.invalid\n\n<script>raw</script>\n" +
        std::wstring(L"\n[first]: /first 'first title'\n[second]: /second 'second title'\n| - |\n") + std::wstring(512u, L'\n') + L"&NotEqualTilde; &#x1f600;";
    Document sourceDocument(source);
    const auto sourceSnapshot = sourceDocument.Snapshot();
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
        parser.Begin(sourceSnapshot, 2u, MarkdownProfile::Gfm029);
        finish(parser);
        countAllocations = false;
        requests         = allocations;
    }
    Check(requests > 0u && requests < 2048u, "finite observed schedule includes inline links/styles, tables, tasks, tagfilter and compaction");
    if (trace)
        std::fprintf(stderr, "lossless Markdown uses isolated fatal allocation probe and checked admission\n");
    size_t beginRequests = 0u;
    {
        MarkdownParser parser;
        parser.Begin(L"old pending", 1u);
        parser.Advance(1u);
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
        parser.Begin(sourceSnapshot, 2u, MarkdownProfile::Gfm029);
        countAllocations = false;
        beginRequests    = allocations;
    }
    Check(beginRequests > 0u && beginRequests < requests, "Begin allocation schedule is independently observed");
    {
        MarkdownParser parser;
        parser.Begin(L"published", 1u);
        finish(parser);
        const auto published = parser.Published();
        parser.Begin(L"old pending", 2u);
        parser.Advance(1u);
        const auto rejected = parser.TryBegin(sourceSnapshot, 3u, static_cast<MarkdownProfile>(99));
        Check(rejected == MarkdownResult::InvalidProfile && parser.HasPendingPreparation() && parser.Published() == published,
              "checked Begin refusal preserves both an existing pending request and the previous published identity");
        finish(parser);
        Check(parser.Published()->Revision() == 2u && parser.Published()->Source() == L"old pending",
              "the original pending cursor can finish after replacement admission is refused");
    }
    const std::wstring maximum(Document::MaximumUnits, L'\n');
    ptrdiff_t retained = 0, released = 0, peak = 0;
    {
        const auto oldCounting = countLiveBytes;
        const auto oldLive     = liveBytes;
        auto* const oldPeak    = liveBytesPeak;
        const auto restore     = wil::scope_exit([=] noexcept
        {
            countLiveBytes = oldCounting;
            liveBytes      = oldLive;
            liveBytesPeak  = oldPeak;
        });
        liveBytes              = 0;
        countLiveBytes         = true;
        liveBytesPeak          = &peak;
        {
            MarkdownParser parser;
            parser.Begin(maximum, 1u);
            finish(parser);
            Check(parser.Published()->Blocks().size() == maximum.size() && parser.Published()->Lexemes().size() == maximum.size(),
                  "maximum physical-line fixture publishes all partitions without truncation");
            retained = liveBytes;
            for (std::uint64_t revision = 2u; revision <= 4u; ++revision)
            {
                parser.Begin(maximum, revision);
                parser.Advance(17u);
                Check(parser.Published()->Revision() == revision - 1u, "maximum replacement keeps its previous publication until complete");
                finish(parser);
                Check(parser.Published()->Revision() == revision, "maximum replacement publishes its own completed revision");
            }
        }
        released       = liveBytes;
        countLiveBytes = false;
    }
    std::cout << "lossless Markdown maximum storage retainedCppBytes=" << retained << " replacementPeakCppBytes=" << peak << " releasedDelta=" << released
              << '\n';
    Check(retained >= 0 && retained <= 24 * 1024 * 1024 && released == 0, "maximum bounded syntax storage releases completely within 24 MiB C++ budget");
    Check(peak >= retained && peak <= 32 * 1024 * 1024, "old/staged maximum syntax replacement stays within 32 MiB peak C++ storage");
    MarkdownParser parser;
    parser.Begin(sourceSnapshot, 1u, MarkdownProfile::Gfm029);
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
            parser.Begin(sourceSnapshot, 1u, MarkdownProfile::Gfm029);
            static_cast<void>(parser.Published());
            parser.Advance(0u);
            parser.Cancel();
        }
        parser.Clear();
        countAllocations = false;
        idleCalls        = allocations;
    }
    Check(idleCalls == 0u, "same-content preparation, completed reads and retirement allocate no C++ storage");
    std::cout << "lossless Markdown measuredRequests=" << requests << " pendingBeginRequests=" << beginRequests << " maximumRetainedCppBytes=" << retained
              << " replacementPeakCppBytes=" << peak << " releasedDelta=" << released << " unchangedAndRetireAllocations=" << idleCalls << '\n';
}

static void TestEmbeddedFootnoteBlockSourceEdits(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "footnote borrowed WARP resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "footnote embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    for (const auto& scenario : FootnoteBlockTests::scenarios)
    {
        editor->SetText(std::wstring(scenario.source));
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded note block preparation");
        const auto syntax = editor->GetMarkdownSyntax();
        Check(syntax != nullptr, "embedded note source preparation completes");
        FootnoteBlockTests::CheckSyntax(*syntax, scenario, Check);
        const auto semantic = editor->GetAcknowledgedSemanticDocument(view.Controls());
        Check(semantic && semantic->Syntax() == syntax && editor->GetText() == scenario.source && ! editor->CanUndo() && ! view.Controls().GetHwnd(),
              "embedded note syntax shares source and history without a control HWND");
    }
    const std::wstring source = L"[^n]: first\r\n\r\n    second\r\n";
    editor->SetText(source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded note edit baseline");
    const auto at = source.find(L"second");
    editor->SetSelection({at, at + 6u});
    for (const auto command : {MarkdownCommand::ToggleStyle, MarkdownCommand::SetHeading, MarkdownCommand::SetList, MarkdownCommand::InsertTable})
        Check(editor->ExecuteMarkdownCommand({command}) == MarkdownCommandStatus::Unavailable && editor->GetText() == source && ! editor->CanUndo(),
              "initial embedded literal note commands reject child prefix reinterpretation atomically");
    Check(editor->ReplaceSelection(L"\u03b2") && editor->Undo() && editor->GetText() == source && editor->Redo(),
          "raw embedded note replacement remains one reversible Unicode transaction");
}

static void TestEmbeddedFootnoteResolutionAndSourceEdits(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "footnote binding borrowed WARP resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "footnote binding embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    FootnoteResolutionTests::ExerciseSourceEdits(*editor,
                                                 [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded note resolution preparation"); },
                                                 [&](const auto& syntax)
    {
        const auto semantic = editor->GetAcknowledgedSemanticDocument(view.Controls());
        if (semantic)
            FootnoteResolutionTests::CheckSemantic(*semantic, Check);
        return semantic && semantic->Syntax() == syntax && semantic->Map().Source() == syntax->Source() && ! view.Controls().GetHwnd();
    },
                                                 Check);
    FootnoteResolutionTests::ExerciseNavigation(
        *editor, view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded footnote navigation"); }, Check, MK_CONTROL, MK_SHIFT);
}

template <typename Operation> __declspec(noinline) static void RichParserPreparationFaultOrigin(Operation&& operation)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    operation();
}
static void TestParserCapacityMemoryFailureIsRetryable(GraphicsFixture& gpu, bool fatalChild = false)
{
    using namespace DxUi;
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
    class PreparationProbe final : public RichTextEditor
    {
    public:
        using RichTextEditor::PrepareContent;
        using RichTextEditor::RichTextEditor;
    };
#pragma warning(pop)
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "capacity memory probe borrowed device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "capacity memory probe host");
    auto owned   = std::make_unique<PreparationProbe>(L"old");
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "capacity memory previous publication");
    const auto previous = editor->GetMarkdownSyntax();
    editor->SetText(L"new **small**");
    editor->SetSelection({1u, 2u});
    if (fatalChild)
    {
        RichParserPreparationFaultOrigin([&] { (void)editor->PrepareContent(view.Controls()); });
        return;
    }
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-parser-preparation-allocation"), "parser preparation allocation terminates in its owned child");
    Check(editor->GetText() == L"new **small**" && editor->GetSelection() == RichText::Selection{1u, 2u} && ! editor->CanUndo() && previous->Source() == L"old",
          "pending parser preparation preserves the source and held publication");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare changed parser source");
    Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && editor->GetMarkdownSyntax()->Source() == L"new **small**",
          "changed source publishes its semantic syntax");
}
static void TestEmbeddedParserCapacitySourceFallback(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    MarkdownCapacityTests::ParserBounds(Check);
    MarkdownCapacityTests::AutolinkSuffixes(Check);
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "capacity fallback borrowed device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "capacity fallback embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    MarkdownCapacityTests::EditorFallback(
        *editor, view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "capacity source fallback preparation"); }, Check);
    Check(! view.Controls().GetHwnd(), "capacity fallback acquires no control HWND");
}
static void TestEmbeddedLocalizedRecordedServiceCommit(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "recorded service shared borrowed device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "recorded service embedded owner");
    auto owned   = std::make_unique<RichTextStateReplacementTests::ServiceEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    RichTextStateReplacementTests::Service(*editor, view.Controls(), Check);
    Check(! view.Controls().GetHwnd(), "recorded embedded import needs no control HWND");
}
static void TestEmbeddedImageResources(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "image resource shared device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "image resource embedded owner");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    const auto prepare = [&] { Hr(PrepareRichEmbeddedContent(view, gpu.width, gpu.height), "image resource owner preparation"); };
    RichImageResourceTests::Retained(*editor, view.Controls(), prepare, Check);
    RichImageResourceTests::ReadyPicture(*editor, prepare, Check, true);
    const auto source   = editor->GetText();
    const auto revision = editor->Revision();
    const auto capture  = [&](bool selected = false, float scale = 1.f)
    {
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "image resource actual composition");
        std::vector<std::uint8_t> pixels(0u);
        Hr(gpu.Read(pixels), "image resource actual readback");
        RichImageResourceTests::PatternPixels(*editor, view.Controls(), pixels, gpu.width, gpu.height, scale, Check, selected);
    };
    capture();
    editor->SetSelection({0u, source.size()});
    prepare();
    capture(true);
    editor->SetSelection({0u, 0u});
    prepare();
    gpu.Bind();
    const auto before = allocations;
    countAllocations  = true;
    for (unsigned frame = 0u; frame < 12u; ++frame)
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "warm image composition");
    countAllocations = false;
    Check(allocations == before, "warm image composition allocates no C++ memory or decode graph");
    editor->SetFlowDirection(FlowDirection::RightToLeft);
    prepare();
    capture();
    Hr(PrepareRichEmbeddedContent(view, gpu.width, gpu.height, 144.f), "image raster density transition");
    capture(false, 1.5f);
    prepare();
    capture();
    view.SetVisible(false);
    Check(! editor->DebugGetState().layoutReady && ! view.NeedsPreparation(), "hidden image view releases prepared pixels without work");
    view.SetVisible(true);
    prepare();
    capture();
    std::vector<DxUi::RichText::RichImageRequest> requests(0u);
    editor->TakePendingImageRequests(requests);
    Check(requests.empty(), "image show reuses immutable decoded pixels without another file request");
    GraphicsFixture replacement;
    replacement.width  = gpu.width;
    replacement.height = gpu.height;
    Hr(replacement.Create(), "image replacement WARP device");
    std::shared_ptr<GraphicsDevice> next;
    Hr(GraphicsDevice::Create(replacement.device.get(), next), "image replacement shared pool");
    Hr(view.ReplaceDevice(next), "image resource device replacement");
    prepare();
    replacement.Bind();
    Hr(view.Composite(replacement.context.get(), replacement.Viewport()), "image after replacement composition");
    std::vector<std::uint8_t> pixels(0u);
    Hr(replacement.Read(pixels), "image after replacement readback");
    RichImageResourceTests::PatternPixels(*editor, view.Controls(), pixels, replacement.width, replacement.height, 1.f, Check);
    editor->TakePendingImageRequests(requests);
    Check(requests.empty() && editor->GetText() == source && editor->Revision() == revision && ! editor->CanUndo(),
          "device replacement rebuilds GPU images from held pixels without source/history or repeated I/O");
    constexpr std::string_view svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 20 20'><rect width='20' height='20' fill='red'/></svg>";
    std::shared_ptr<const RichText::RichImage> vectorImage;
    Hr(RichText::RichImage::CreateSvg(20u, 20u, std::as_bytes(std::span(svg)), vectorImage), "create immutable SVG fixture");
    editor->SetFlowDirection(FlowDirection::LeftToRight);
    editor->SetSvgImagesEnabled(true);
    editor->SetText(L"![vector](shape.svg)\n");
    prepare();
    editor->TakePendingImageRequests(requests);
    Check(requests.size() == 1u && editor->CompleteImageRequest(requests.front(), vectorImage), "publish SVG fixture to embedded owner");
    prepare();
    const auto verifySvg = [&](GraphicsFixture& fixture, bool selected = false)
    {
        fixture.Bind();
        Hr(view.Composite(fixture.context.get(), fixture.Viewport()), "SVG actual composition");
        std::vector<std::uint8_t> data(0u);
        Hr(fixture.Read(data), "SVG actual pixel readback");
        const auto document = editor->GetAcknowledgedSemanticDocument(view.Controls());
        Check(static_cast<bool>(document), "SVG has acknowledged semantic geometry");
        const auto image = std::ranges::find(document->Elements(), RichText::SemanticRole::Image, &RichText::SemanticElement::role);
        Check(image != document->Elements().end(), "SVG semantic image remains present");
        const auto bounds = editor->GetSemanticElementBounds(view.Controls(), static_cast<std::size_t>(image - document->Elements().begin()));
        Check(bounds && std::abs(bounds->right - bounds->left - 20.f) < .02f, "SVG prepared geometry uses nominal dimensions");
        const auto x = static_cast<unsigned>((bounds->left + bounds->right) * .5f), y = static_cast<unsigned>((bounds->top + bounds->bottom) * .5f);
        Check(x < fixture.width && y < fixture.height, "SVG oracle samples within the target");
        const auto offset = (static_cast<std::size_t>(y) * fixture.width + x) * 4u;
        const bool red    = data[offset] < 5u && data[offset + 1u] < 5u && data[offset + 2u] > 250u && data[offset + 3u] > 250u;
        Check(selected ? ! red : red, selected ? "SVG selection applies prepared image opacity" : "SVG pixels are rendered by the prepared Direct2D document");
    };
    verifySvg(replacement);
#if DXUI_ENABLE_DIAGNOSTICS
    const auto svgRasterizations = RichText::Detail::testRichSvgRasterizations;
    Hr(PrepareRichEmbeddedContent(view, gpu.width - 8u, gpu.height), "SVG width-only reflow");
    Check(RichText::Detail::testRichSvgRasterizations == svgRasterizations,
          "width-only reflow reuses the same target-generation SVG bitmap when pixel dimensions are unchanged");
    prepare();
    Check(RichText::Detail::testRichSvgRasterizations == svgRasterizations, "restoring width reuses the prepared SVG bitmap");
#endif
    editor->SetSelection({0u, editor->GetText().size()});
    prepare();
    verifySvg(replacement, true);
    editor->SetSelection({0u, 0u});
    prepare();
    const auto svgRevision = editor->Revision();
    Hr(view.ReplaceDevice(graphics), "replace SVG target device generation");
    prepare();
    verifySvg(gpu);
    editor->TakePendingImageRequests(requests);
    Check(requests.empty() && editor->Revision() == svgRevision && editor->GetText() == L"![vector](shape.svg)\n",
          "SVG device replacement recreates device resources from held bytes without re-requesting source");
    gpu.Bind();
    const auto svgAllocations = allocations;
    countAllocations          = true;
    for (unsigned frame = 0u; frame < 12u; ++frame)
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "warm SVG composition");
    countAllocations = false;
    Check(allocations == svgAllocations, "warm SVG draw performs no C++ allocations");
    constexpr std::wstring_view sizedSvgSource = L"<img src='shape.svg' alt='vector' width='48' height='40'>\n";
    editor->SetText(std::wstring(sizedSvgSource));
    prepare();
    editor->TakePendingImageRequests(requests);
    if (! requests.empty())
    {
        Check(requests.size() == 1u && editor->CompleteImageRequest(requests.front(), vectorImage), "publish authored-size SVG resource");
        prepare();
    }
    const auto sizedDocument = editor->GetAcknowledgedSemanticDocument(view.Controls());
    Check(static_cast<bool>(sizedDocument), "authored SVG has current semantic geometry");
    const auto sizedImage = std::ranges::find(sizedDocument->Elements(), RichText::SemanticRole::Image, &RichText::SemanticElement::role);
    Check(sizedImage != sizedDocument->Elements().end(), "authored SVG retains its image owner");
    const auto sizedBounds = editor->GetSemanticElementBounds(view.Controls(), static_cast<std::size_t>(sizedImage - sizedDocument->Elements().begin()));
    Check(sizedBounds && std::abs(sizedBounds->right - sizedBounds->left - 48.f) < .02f && std::abs(sizedBounds->bottom - sizedBounds->top - 40.f) < .02f,
          "authored SVG uses its non-square 48 by 40 geometry");
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "compose authored SVG geometry");
    std::vector<std::uint8_t> sizedPixels(0u);
    Hr(gpu.Read(sizedPixels), "read authored SVG pixels");
    for (const auto [dx, dy] : std::array<std::pair<float, float>, 4u>{{{4.f, 4.f}, {44.f, 4.f}, {4.f, 36.f}, {44.f, 36.f}}})
    {
        const auto x = static_cast<unsigned>(sizedBounds->left + dx), y = static_cast<unsigned>(sizedBounds->top + dy);
        Check(x < gpu.width && y < gpu.height, "authored SVG edge oracle samples within the target");
        const auto offset = (static_cast<std::size_t>(y) * gpu.width + x) * 4u;
        Check(sizedPixels[offset] < 5u && sizedPixels[offset + 1u] < 5u && sizedPixels[offset + 2u] > 250u && sizedPixels[offset + 3u] > 250u,
              "SVG fills each authored edge rather than retaining its nominal viewport extent");
    }
    Check(editor->GetText() == sizedSvgSource, "authored SVG rendering preserves exact HTML source");
}
static void TestEmbeddedImagePathPaste(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "image-paste shared device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "image-paste embedded owner");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    RichImagePathTests::Paths(Check);
    RichImagePathTests::Codec(Check);
    RichImagePathTests::Retained(*editor, [&] { Hr(PrepareRichEmbeddedContent(view, 640u, 240u), "image paste preparation"); }, Check);
    view.Controls().SetFocusControl(editor, false);
    editor->SetText(L"ok ");
    editor->SetSelection({3u, 3u});
    Hr(PrepareRichEmbeddedContent(view, 640u, 240u), "image paste transport preparation");
    EmbeddedTextInputSnapshot snapshot;
    Hr(view.ReadTextInput(snapshot), "image paste reads expected revision");
    TextClipboardContent content;
    content.text             = L"C:\\docs\\a.png";
    std::wstring replacement = L"sentinel";
    Check(view.TransformClipboardPaste(snapshot.revision, snapshot.focusId, content, replacement) == S_OK && replacement == L"![a](a.png)",
          "embedded clipboard preparation uses source snapshot and document location");
    editor->SetDocumentFilePath(L"D:\\note.md");
    Check(view.TransformClipboardPaste(snapshot.revision, snapshot.focusId, content, replacement) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH),
          "document-location change rejects stale embedded clipboard preparation");
    Check(editor->GetText() == L"ok " && ! editor->CanUndo() && ! view.Controls().GetHwnd(), "embedded transform creates no HWND or source transaction");
}
__declspec(noinline) static void RichPasteTransformFaultOrigin(DxUi::RichTextEditor& editor,
                                                               const DxUi::TextClipboardContent& content,
                                                               std::wstring& replacement)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)editor.TryPrepareClipboardPaste(content, replacement);
}
static void TestEmbeddedFileReferencePaste(GraphicsFixture& gpu, unsigned fatalScenario = 0u)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "file reference shared device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "file reference embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    RichImagePathTests::FileReferences(Check);
    RichImagePathTests::RetainedFileReferences(*editor, [&] { Hr(PrepareRichEmbeddedContent(view, 640u, 240u), "file reference preparation"); }, Check);
    Check(! view.Controls().GetHwnd(), "file reference paste creates no embedded HWND");
    editor->SetText(L"replace");
    editor->SetSelection({7u, 0u});
    Hr(PrepareRichEmbeddedContent(view, 640u, 240u), "mixed paste failure fixture");
    TextClipboardContent content;
    content.filePaths    = {L"C:\\docs\\a.png", L"C:\\docs\\report.pdf", L"D:\\other.txt"};
    const auto source    = editor->GetText();
    const auto revision  = editor->Revision();
    const auto selection = editor->GetSelection();
    std::wstring expected(0u, L' ');
    std::size_t requests{};
    {
        const bool oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        const bool transformed        = editor->TryPrepareClipboardPaste(content, expected);
        countAllocations              = false;
        requests                      = allocations;
        Check(transformed && requests > 0u && requests < 512u, "mixed file paste has an observed bounded allocation schedule");
    }
    {
        std::wstring replacement = L"sentinel";
        if (fatalScenario == 1u)
        {
            RichPasteTransformFaultOrigin(*editor, content, replacement);
            return;
        }
        if (fatalScenario == 0u)
            Check(DxUiFailureTests::ExpectFatalChild(L"embedded-mixed-paste-allocation"), "mixed paste allocation terminates in its owned child");
        Check(editor->TryPrepareClipboardPaste(content, replacement) && replacement == expected && editor->GetText() == source &&
                  editor->Revision() == revision && editor->GetSelection() == selection && ! editor->CanUndo(),
              "mixed paste preserves source, selection and history");
    }
    std::cout << "mixed file paste allocation points=" << requests << '\n';
    std::wstring deepPath = L"C:";
    for (std::size_t i = 0u; i < 513u; ++i)
        deepPath.append(L"\\a");
    content.filePaths = {L"C:\\docs\\a.png", deepPath};
    expected.clear();
    {
        const bool oldCounting = countAllocations;
        const auto oldCalls = allocations, oldBytes = allocationBytes;
        const auto restore = wil::scope_exit([=] noexcept
        {
            countAllocations = oldCounting;
            allocations      = oldCalls;
            allocationBytes  = oldBytes;
        });
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        const bool transformed        = editor->TryPrepareClipboardPaste(content, expected);
        countAllocations              = false;
        requests                      = allocations;
        Check(transformed && requests > 0u && requests < 512u, "deep literal paste has an observed bounded allocation schedule");
    }
    {
        std::wstring replacement = L"sentinel";
        if (fatalScenario == 2u)
        {
            RichPasteTransformFaultOrigin(*editor, content, replacement);
            return;
        }
        if (fatalScenario == 0u)
            Check(DxUiFailureTests::ExpectFatalChild(L"embedded-deep-paste-allocation"), "deep paste allocation terminates in its owned child");
        Check(editor->TryPrepareClipboardPaste(content, replacement) && replacement == expected && editor->GetText() == source &&
                  editor->Revision() == revision && editor->GetSelection() == selection && ! editor->CanUndo(),
              "deep paste preserves source, selection and history");
    }
    if (fatalScenario != 0u)
        return;
    BITMAPV5HEADER header{};
    header.bV5Size        = sizeof(header);
    header.bV5Width       = 2;
    header.bV5Height      = 2;
    header.bV5Planes      = 1u;
    header.bV5BitCount    = 32u;
    header.bV5Compression = BI_RGB;
    header.bV5SizeImage   = 16u;
    TextClipboardContent bitmap;
    bitmap.bitmapV5 = true;
    bitmap.bitmapDib.resize(sizeof(header) + 16u);
    std::memcpy(bitmap.bitmapDib.data(), &header, sizeof(header));
    view.Controls().SetFocusControl(editor, false);
    Hr(PrepareRichEmbeddedContent(view, 640u, 240u), "bitmap paste preparation");
    EmbeddedTextInputSnapshot snapshot;
    Hr(view.ReadTextInput(snapshot), "bitmap paste focus snapshot");
    unsigned bitmapCalls{};
    editor->SetBitmapPasteHandler([&](std::span<const std::byte> data, bool v5) -> std::optional<std::wstring>
    {
        ++bitmapCalls;
        Check(v5 && std::ranges::equal(data, bitmap.bitmapDib), "embedded asset handler receives exact validated DIBV5 bytes");
        return L"C:\\docs\\bitmap.png";
    });
    Check(view.ApplyClipboardBitmapPaste(snapshot.revision + 1u, snapshot.focusId, bitmap) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH) && bitmapCalls == 0u,
          "stale embedded bitmap paste refuses before the asset callback");
    Check(view.ApplyClipboardBitmapPaste(snapshot.revision, snapshot.focusId, bitmap) == S_OK && editor->GetText() == L"![bitmap](bitmap.png)" &&
              bitmapCalls == 1u,
          "embedded bitmap paste commits a saved asset once without an HWND");
    Check(editor->Undo() && editor->GetText() == source && editor->GetSelection() == selection,
          "embedded bitmap paste restores exact source and selection in one undo");
}
static void TestEmbeddedRetainedObjectSourceEditing(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "source editing borrowed device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "source editing embedded owner");
    auto owned   = std::make_unique<SemanticSourceBlockTests::SourceEditingEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    SemanticSourceBlockTests::Retained(
        *editor, view.Controls(), [&] { Hr(PrepareRichEmbeddedContent(view, 640u, 240u), "source editing owner preparation"); }, Check);
    Check(! view.Controls().GetHwnd(), "retained source editing creates no embedded control HWND");
}
static void TestEmbeddedDiagramSelectionRaster(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "native selection borrowed device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "native selection embedded host");
    view.Controls().SetTheme(SemanticDiagramTests::SelectionPalette());
    auto owned   = std::make_unique<RichTextEditor>(std::wstring(SemanticDiagramTests::SelectionSource));
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "native selection embedded preparation");
    editor->SetSelection({0u, editor->GetText().size()});
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "native selection embedded selected preparation");
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "native selection embedded composite");
    std::vector<uint8_t> pixels;
    Hr(gpu.Read(pixels), "native selection embedded pixels");
    SemanticDiagramTests::SelectionPixels(*editor, view.Controls(), pixels, gpu.width, gpu.height, 1.f, Check);
}
static void TestEmbeddedFlowchartRenderingAndFallbacks(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    NativeFlowchartTests::CheckOriginalCorpus(Check);
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "diagram borrowed WARP resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "diagram embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    for (const auto& scenario : NativeFlowchartTests::scenarios)
    {
        const auto source = NativeFlowchartTests::FencedSource(scenario);
        editor->SetText(source);
        Hr(PrepareRichEmbeddedContent(view, 1024u, 240u), "embedded diagram definition preparation");
        const auto syntax = editor->GetMarkdownSyntax();
        Check(syntax != nullptr, "embedded diagram source preparation completes");
        NativeFlowchartTests::CheckFencedSource(*syntax, scenario, Check);
        const auto semantic = editor->GetAcknowledgedSemanticDocument(view.Controls());
        Check(semantic && semantic->Syntax() == syntax && semantic->Map().Source() == source && editor->GetText() == source && ! editor->CanUndo() &&
                  ! view.Controls().GetHwnd(),
              "embedded graph rendering/fallback preserves source and history without a control HWND");
        Check(scenario.outcome == RichText::Detail::FlowchartOutcome::Prepared ? ! RichText::Detail::SemanticExtensionAccess::Diagrams(*semantic).empty()
                                                                               : semantic->Map().Display() == NativeFlowchartTests::LiteralDisplay(scenario),
              "embedded admitted graphs render atomically and rejected diagrams keep literal code");
        editor->SetSelection({13u, 13u});
        Check(editor->ReplaceSelection(L"\u03b2") && editor->Undo() && editor->GetText() == source,
              "every embedded diagram definition remains a reversible Unicode source edit");
    }
}

static void TestEmbeddedDiagramViewCapacityRecovery(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "native diagram recovery resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "native diagram recovery embedding");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    SemanticDiagramTests::Editor(
        *editor, view.Controls(), [&](UINT width) { Hr(PrepareRichEmbeddedContent(view, width, 240u), "embedded diagram recovery preparation"); }, Check);
    Check(! view.Controls().GetHwnd(), "diagram recovery uses borrowed hosting services without a child HWND");
}
static void TestEmbeddedFrontMatterFencesAndSourceEdits(GraphicsFixture& gpu)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "metadata borrowed WARP resources");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "metadata embedded host");
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(owned));
    for (const auto& scenario : FrontMatterTests::scenarios)
    {
        editor->SetText(std::wstring(scenario.source));
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded metadata preparation");
        const auto syntax = editor->GetMarkdownSyntax();
        Check(syntax != nullptr, "embedded metadata source preparation completes");
        FrontMatterTests::CheckSyntax(*syntax, scenario, Check);
        const auto semantic = editor->GetAcknowledgedSemanticDocument(view.Controls());
        Check(semantic && semantic->Syntax() == syntax && editor->GetText() == scenario.source && ! editor->CanUndo() && ! view.Controls().GetHwnd(),
              "embedded metadata shares acknowledged syntax and exact source/history without a control HWND");
    }
    const std::wstring source = L"---\r\n# keep comment\r\nname: '\u03b2'\r\n---\r\n# Body\r\n";
    editor->SetText(source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded metadata edit baseline");
    const auto held = editor->GetAcknowledgedSemanticDocument(view.Controls());
    const auto at   = source.find(L'\u03b2');
    editor->SetSelection({at, at + 1u});
    for (const auto command : {MarkdownCommand::ToggleStyle, MarkdownCommand::SetHeading, MarkdownCommand::InsertTable})
        Check(editor->ExecuteMarkdownCommand({command}) == MarkdownCommandStatus::Unavailable && editor->GetText() == source && ! editor->CanUndo(),
              "embedded structural commands cannot rewrite opaque metadata");
    Check(editor->ReplaceSelection(L"\u03a9") && ! editor->GetAcknowledgedSemanticDocument(view.Controls()),
          "embedded metadata edit immediately rejects stale semantic acknowledgement");
    auto expected = source;
    expected.replace(at, 1u, L"\u03a9");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded metadata source edit");
    Check(editor->GetText() == expected && held->Syntax()->Source() == source, "embedded metadata source edits preserve untouched text and held revisions");
    Check(editor->Undo() && editor->GetText() == source && ! editor->CanUndo() && editor->Redo() && editor->GetText() == expected,
          "embedded metadata edit is exactly one undo/redo transaction");
    const auto selection = editor->GetSelection();
    editor->SetViewMode(ViewMode::Source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded metadata source view");
    Check(editor->GetSelection() == selection && editor->GetText() == expected, "embedded source view retains metadata spelling and selection");
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
    editor->SetText(L"> [a]: /u\n> text\n");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded reference and paragraph prefixes");
    const auto referenceSyntax = editor->GetMarkdownSyntax();
    Check(referenceSyntax != nullptr, "embedded reference prefix syntax is published");
    MarkdownBlockTests::CheckReferencePrefix(*referenceSyntax, Check);
    editor->SetText(L"> [a]: /u\r\n> [b]: /v\r\n> text\r\n");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded consecutive reference prefixes");
    const auto referencesSyntax = editor->GetMarkdownSyntax();
    Check(referencesSyntax != nullptr, "embedded consecutive-reference syntax is published");
    MarkdownBlockTests::CheckReferencePrefixesCrLf(*referencesSyntax, Check);
    for (const auto profile : {MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
        for (const auto& scenario : MarkdownBlockTests::splitTables)
        {
            editor->SetMarkdownAnalysisProfile(profile);
            editor->SetText(std::wstring(scenario.source));
            Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded split table header");
            const auto parsed = editor->GetMarkdownSyntax();
            Check(parsed != nullptr, "embedded split-table syntax is published");
            MarkdownBlockTests::CheckSplitTable(*parsed, scenario, Check);
        }
    for (const auto profile : {MarkdownProfile::Gfm029, MarkdownProfile::CommonMark0312, MarkdownProfile::RichMarkdown})
    {
        editor->SetMarkdownAnalysisProfile(profile);
        editor->SetText(L"> a\n<custom>");
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded profile-specific lazy HTML");
        const auto parsed = editor->GetMarkdownSyntax();
        Check(parsed != nullptr, "embedded lazy HTML syntax is published");
        MarkdownBlockTests::CheckLazyHtmlProfile(*parsed, Check);
    }
    for (const auto profile : {MarkdownProfile::Gfm029, MarkdownProfile::CommonMark0312, MarkdownProfile::RichMarkdown})
        for (const bool uppercase : {false, true})
        {
            editor->SetMarkdownAnalysisProfile(profile);
            editor->SetText(uppercase ? L"<!DOCTYPE html>\nfoo" : L"<!doctype html>\nfoo");
            Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded profile-specific declaration");
            const auto parsed = editor->GetMarkdownSyntax();
            Check(parsed != nullptr, "embedded declaration syntax is published");
            MarkdownBlockTests::CheckDeclarationProfile(*parsed, uppercase, Check);
        }
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    for (const auto& scenario : MarkdownBlockTests::scenarios)
    {
        editor->SetText(std::wstring(scenario.source));
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded independent block scenario");
        const auto parsed = editor->GetMarkdownSyntax();
        Check(parsed && MarkdownBlockTests::BlockSignature(*parsed) == scenario.signature, "embedded block grammar matches independent expected structure");
        Check(editor->GetText() == scenario.source && ! editor->CanUndo() && independentEditor->GetMarkdownSyntax() == independentSyntax,
              "embedded block grammar retains exact source/history and isolates its second view");
    }
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        editor->SetMarkdownAnalysisProfile(profile);
        for (const auto& scenario : MarkdownInlineTests::scenarios)
        {
            editor->SetText(std::wstring(scenario.source));
            Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded inline/extension scenario");
            const auto parsed = editor->GetMarkdownSyntax();
            Check(parsed && MarkdownHtmlTests::Html(*parsed) == MarkdownInlineTests::Expected(scenario, profile),
                  "embedded inline/extension semantics match independent expectations");
            Check(editor->GetText() == scenario.source && ! editor->CanUndo() && independentEditor->GetMarkdownSyntax() == independentSyntax,
                  "embedded inline analysis preserves source/history and isolates the other view");
        }
    }
    editor->SetText(std::wstring(MarkdownInlineTests::paddedTable));
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded padded table preparation");
    const auto table = editor->GetMarkdownSyntax();
    const auto cells = MarkdownInlineTests::TableCellIdentities(*table);
    Check(cells.size() == 6u, "embedded table contains both missing body cells");
    editor->SetSelection({editor->GetText().size(), editor->GetText().size()});
    Check(editor->ReplaceSelection(L"outside\n"), "embedded edit outside padded table");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded padded table edit preparation");
    Check(MarkdownInlineTests::UniqueNodeIdentities(*editor->GetMarkdownSyntax()) &&
              MarkdownInlineTests::TableCellIdentities(*editor->GetMarkdownSyntax()) == cells,
          "embedded unrelated edit retains unique source-backed and padded cell identities");
    Check(editor->Undo(), "embedded padded table unrelated edit undo");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded padded table undo preparation");
    Check(MarkdownInlineTests::UniqueNodeIdentities(*editor->GetMarkdownSyntax()) &&
              MarkdownInlineTests::TableCellIdentities(*editor->GetMarkdownSyntax()) == cells,
          "embedded undo retains distinct padded cell identities");
    Check(editor->Redo(), "embedded padded table unrelated edit redo");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded padded table redo preparation");
    Check(MarkdownInlineTests::UniqueNodeIdentities(*editor->GetMarkdownSyntax()) &&
              MarkdownInlineTests::TableCellIdentities(*editor->GetMarkdownSyntax()) == cells && MarkdownInlineTests::UniqueNodeIdentities(*table) &&
              independentEditor->GetMarkdownSyntax() == independentSyntax,
          "embedded redo, held snapshots and second view preserve independent identities");
    editor->SetText(L"[reader]\n\n[reader]: /first\n");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded initial reference semantics");
    const auto oldReference      = editor->GetMarkdownSyntax();
    const auto referenceIdentity = oldReference->Nodes()[oldReference->Nodes()[0u].firstChild].identity;
    const auto referenceAt       = editor->GetText().find(L"/first");
    editor->SetSelection({referenceAt, referenceAt + 6u});
    Check(editor->ReplaceSelection(L"/second"), "embedded global reference edit");
    Check(! editor->GetMarkdownSyntax(), "embedded reference edit hides stale inline semantics");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded changed reference semantics");
    const auto newReference = editor->GetMarkdownSyntax();
    Check(MarkdownHtmlTests::Html(*newReference) == L"<p><a href=\"/second\">reader</a></p>\n" &&
              newReference->Nodes()[newReference->Nodes()[0u].firstChild].identity == referenceIdentity,
          "embedded stable block identity cannot retain old reference semantics");
    Check(editor->Undo(), "embedded reference undo");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded reference undo preparation");
    Check(MarkdownHtmlTests::Html(*editor->GetMarkdownSyntax()) == MarkdownHtmlTests::Html(*oldReference), "embedded undo re-resolves references");
    Check(editor->Redo(), "embedded reference redo");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded reference redo preparation");
    Check(MarkdownHtmlTests::Html(*editor->GetMarkdownSyntax()) == MarkdownHtmlTests::Html(*newReference) &&
              MarkdownHtmlTests::Html(*oldReference) == L"<p><a href=\"/first\">reader</a></p>\n",
          "embedded redo and immutable held reference tree");
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::CommonMark0312);
    editor->SetText(L"# heading\n\n> alpha\n\ntail\n");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded identity initial tree");
    const auto oldTree      = editor->GetMarkdownSyntax();
    const auto originalText = editor->GetText();
    const auto editAt       = originalText.find(L"alpha");
    editor->SetSelection({editAt, editAt});
    Check(editor->ReplaceSelection(L"\U0001f600 "), "embedded Unicode block edit");
    Check(! editor->GetMarkdownSyntax(), "embedded edit hides stale current syntax");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded identity changed tree");
    const auto changedTree = editor->GetMarkdownSyntax();
    Check(changedTree && changedTree->DocumentIdentity() == oldTree->DocumentIdentity() && changedTree->Nodes().size() == oldTree->Nodes().size(),
          "embedded owner publication retains identity scope");
    for (std::size_t i = 0; i < oldTree->Nodes().size(); ++i)
        Check(changedTree->Nodes()[i].identity == oldTree->Nodes()[i].identity, "embedded unchanged/enclosing node reconciliation");
    Check(editor->Undo(), "embedded block edit undo");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded undo tree");
    Check(editor->GetText() == originalText && oldTree->Source() == originalText, "embedded held tree remains immutable through undo");
    editor->SetText(L"same\n\nsame\n\nsame\n");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded duplicate tree");
    const auto duplicates = editor->GetMarkdownSyntax();
    const auto first      = duplicates->Nodes()[0u].firstChild;
    const auto second     = duplicates->Nodes()[first].nextSibling;
    editor->SetSelection({0u, 6u});
    Check(editor->ReplaceSelection(L""), "embedded delete first duplicate");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded reconciled duplicate tree");
    const auto remaining = editor->GetMarkdownSyntax();
    Check(remaining->Nodes()[remaining->Nodes()[0u].firstChild].identity == duplicates->Nodes()[second].identity,
          "embedded tracked ancestry preserves the surviving duplicate's identity");
    std::wstring nested;
    for (unsigned level = 0u; level < 256u; ++level)
        nested += L"- ";
    nested += L"alpha\n";
    nested.append(512u, L' ');
    nested += L"beta\n";
    editor->SetText(nested);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded deep prefix preparation");
    const auto bounded = editor->GetMarkdownSyntax();
    Check(bounded && bounded->Nodes().size() == 514u && bounded->BlockStatistics().prefixInspectionUnits <= nested.size() * 40u,
          "embedded owner preparation keeps deep containers and bounded prefix inspection");
    for (const auto& edge : MarkdownBlockTests::identityEdges)
    {
        editor->SetText(std::wstring(edge.source));
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded paragraph-edge source");
        const auto before = editor->GetMarkdownSyntax();
        editor->SetSelection(edge.selection);
        Check(editor->ReplaceSelection(L""), "embedded Unicode paragraph-edge deletion");
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "embedded paragraph-edge reconciliation");
        const auto after = editor->GetMarkdownSyntax();
        Check(after && after->Nodes()[after->Nodes()[0u].firstChild].identity == before->Nodes()[before->Nodes()[0u].firstChild].identity,
              "embedded first/last scalar deletion preserves its compatible paragraph identity");
    }
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
            catch (const std::bad_alloc&)
            {
                DxUi::Failure::FailAllocation();
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
    Check(pending.preparationPending && pending.lastAdvanceLayouts <= 4u && pending.lastAdvanceBlockSteps <= 4096u &&
              pending.activeStage == DxUi::RichTextPreparationStage::Geometry && pending.lastAdvanceUploadBytes == 0u,
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
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
    class FailingPreparation final : public RichTextEditor
    {
    public:
        FailingPreparation() : RichTextEditor(L"source survives")
        {
        }
        bool fail       = true;
        HRESULT failure = E_OUTOFMEMORY;
        HRESULT PrepareContent(ControlHost& host) noexcept override
        {
            return fail ? failure : RichTextEditor::PrepareContent(host);
        }
    };
#pragma warning(pop)
    EmbeddedHost view;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "failing cooperative graphics");
    size_t requests = 0u;
    Hr(view.Attach(graphics, {&requests, [](void* context) noexcept { ++*static_cast<size_t*>(context); }}), "failing cooperative host");
    auto owned   = std::make_unique<FailingPreparation>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    const auto beforeFailure = requests;
    Check(view.Prepare(480, 240) == E_OUTOFMEMORY && view.NeedsPreparation() && requests == beforeFailure,
          "preparation failure retains its HRESULT and schedules no automatic retry");
    editor->failure = E_ABORT;
    Check(view.Prepare(480, 240) == E_FAIL && view.NeedsPreparation() && requests == beforeFailure,
          "an unchanged content abort is distinct from a superseded frame and does not schedule automatic retry");
    editor->fail = false;
    editor->SetText(L"new source permits retry");
    Check(requests == beforeFailure + 1u, "a new model request wakes a failed dirty view exactly once");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "retry new source after explicit change");
    Check(editor->GetText() == L"new source permits retry" && editor->DebugGetState().layoutReady, "explicit retry publishes retained source");
}

static void TestEmbeddedContinuationCallbackCannotReenterOrUseDetachedState(GraphicsFixture& gpu)
{
    using namespace DxUi;
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
    class OneDeferredPreparation final : public RichTextEditor
    {
    public:
        OneDeferredPreparation() : RichTextEditor(L"deferred source")
        {
        }
        size_t prepareCalls{};
        HRESULT PrepareContent(ControlHost& host) noexcept override
        {
            ++prepareCalls;
            return prepareCalls == 1u ? S_FALSE : RichTextEditor::PrepareContent(host);
        }
    };
#pragma warning(pop)
    struct Requests
    {
        EmbeddedHost* view{};
        size_t count{};
        bool reenter{};
        bool detach{};
        bool detached{};
        HRESULT reentrantResult = S_OK;
    };
    const auto request = [](void* context) noexcept
    {
        auto& state = *static_cast<Requests*>(context);
        ++state.count;
        if (std::exchange(state.reenter, false))
            state.reentrantResult = state.view->Prepare(480u, 240u);
        if (std::exchange(state.detach, false))
        {
            state.view->Detach();
            state.detached = true;
        }
    };

    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "continuation reentry graphics");

    Requests reentrant;
    EmbeddedHost reentrantView;
    reentrant.view = &reentrantView;
    Hr(reentrantView.Attach(graphics, {&reentrant, request}), "continuation reentry host");
    auto reentrantEditor     = std::make_unique<OneDeferredPreparation>();
    auto* reentrantEditorRaw = reentrantEditor.get();
    reentrantView.Controls().SetRoot(std::move(reentrantEditor));
    const size_t beforeReentry = reentrant.count;
    reentrant.reenter          = true;
    Check(reentrantView.Prepare(480u, 240u) == S_FALSE && reentrantView.NeedsPreparation() && reentrant.count == beforeReentry + 1u &&
              reentrant.reentrantResult == E_UNEXPECTED && reentrantEditorRaw->prepareCalls == 1u,
          "one continuation wake rejects recursive preparation without advancing a second turn");
    Hr(PrepareRichEmbeddedContent(reentrantView, 480u, 240u), "service deferred continuation after callback returns");
    Check(reentrantEditorRaw->prepareCalls == 2u && ! reentrantView.NeedsPreparation(), "the owner can service the continuation on a later turn");

    Requests detached;
    EmbeddedHost detachedView;
    detached.view = &detachedView;
    Hr(detachedView.Attach(graphics, {&detached, request}), "continuation detach host");
    auto detachedEditor = std::make_unique<OneDeferredPreparation>();
    detachedView.Controls().SetRoot(std::move(detachedEditor));
    const size_t beforeDetach = detached.count;
    detached.detach           = true;
    Check(detachedView.Prepare(480u, 240u) == S_FALSE && detached.detached && detached.count == beforeDetach + 1u,
          "the continuation callback may detach its host");
    Check(detachedView.Prepare(480u, 240u) == E_UNEXPECTED, "detached continuation cleanup does not reuse retired host state");
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
            std::wstring output(0u, L'\0');
            lengthRejected   = DxUi::RichText::TryDecodeUtf8(input, output) == DxUi::RichText::SourceResult::CapacityExceeded;
            countAllocations = false;
            requestedBytes   = allocationBytes;
        }
        std::cout << "rich UTF8 rejection inputBytes=" << inputBytes << " decoderCppRequestedBytes=" << requestedBytes << '\n';
        Check(lengthRejected, "oversized UTF8 source returns a capacity result");
        Check(requestedBytes <= 4096u, "oversized UTF8 rejection never materializes the oversized UTF16 result");
    }
}

template <typename Operation> __declspec(noinline) static void RichUiaRangeFaultOrigin(Operation&& operation)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)operation();
}
static void TestRetainedRichTextUiaRangeAllocationBudget(GraphicsFixture& gpu, unsigned fatalScenario = 0u)
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
    // The parent retains both metadata budgets; a fault child needs only its
    // selected operation and must not repeat maximum-document setup.
    if (fatalScenario == 0u)
    {
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
    editor->SetText(std::wstring(96u, L'a') + L"\nsecond visual line\nthird line");
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "prepare observed UIA range-failure source");
    Hr(view.UpdateAccessibility(placement), "publish observed UIA range-failure source");
    const auto makeRange = [&]
    {
        wil::com_ptr_nothrow<ITextRangeProvider> range;
        Hr(text->get_DocumentRange(range.put()), "create independent range outside failure schedule");
        Hr(range->MoveEndpointByRange(TextPatternRangeEndpoint_End, range.get(), TextPatternRangeEndpoint_Start), "collapse failure-fixture range");
        int moved{};
        Hr(range->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 4, &moved), "select failure-fixture prefix");
        Check(moved == 4, "allocation fixture has a real nonempty range");
        return range;
    };
    const auto baseline = makeRange();
    const auto invoke   = [&](ITextRangeProvider& range, unsigned operation)
    {
        int value{};
        wil::unique_bstr resultText;
        SAFEARRAY* rectangles = nullptr;
        const auto release    = wil::scope_exit([&] noexcept
        {
            if (rectangles)
                SafeArrayDestroy(rectangles);
        });
        switch (operation)
        {
            case 0u: return range.CompareEndpoints(TextPatternRangeEndpoint_End, baseline.get(), TextPatternRangeEndpoint_Start, &value);
            case 1u: return range.ExpandToEnclosingUnit(TextUnit_Line);
            case 2u: return range.GetBoundingRectangles(&rectangles);
            case 3u: return range.GetText(-1, resultText.put());
            case 4u: return range.Move(TextUnit_Line, 1, &value);
            case 5u: return range.MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Line, 1, &value);
            default: return E_INVALIDARG;
        }
    };
    for (unsigned operation = 0u; operation < 6u; ++operation)
    {
        if (fatalScenario != 0u && fatalScenario != operation + 1u)
            continue;
        auto warm = makeRange();
        Hr(invoke(*warm.get(), operation), "warm retained UIA geometry outside the observed failure schedule");
        auto measured = makeRange();
        allocations = allocationBytes = 0u;
        countAllocations              = true;
        const HRESULT result          = invoke(*measured.get(), operation);
        countAllocations              = false;
        const size_t observed         = allocations;
        Hr(result, "observe successful direct UIA range operation");
        Check(observed > 0u && observed < 256u, "direct UIA range command has a bounded observed allocation schedule");
        auto range = makeRange();
        if (fatalScenario != 0u)
        {
            RichUiaRangeFaultOrigin([&] { return invoke(*range.get(), operation); });
            return;
        }
        constexpr const wchar_t* children[] = {L"embedded-uia-range-compare",
                                               L"embedded-uia-range-expand",
                                               L"embedded-uia-range-rectangles",
                                               L"embedded-uia-range-text",
                                               L"embedded-uia-range-move",
                                               L"embedded-uia-range-endpoint"};
        Check(DxUiFailureTests::ExpectFatalChild(children[operation]), "UIA range operation allocation terminates in its owned child");
        Hr(invoke(*range.get(), operation), "direct UIA range command succeeds");
        std::cout << "rich UIA range operation=" << operation << " allocationRequests=" << observed << '\n';
    }
}

__declspec(noinline) static void RichTaskPointerFaultOrigin(DxUi::RichTextEditor& editor, DxUi::EmbeddedHost& view, D2D1_POINT_2F point)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)editor.OnMouseDown(view.Controls(), point, false, 0u);
}
static void TestRichTaskPlanAllocationFailureKeepsPointerSelection(GraphicsFixture& gpu, bool fatalChild = false)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "task allocation fixture device");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "task allocation fixture host");
    auto root    = std::make_unique<RichTextEditor>(L"- [ ] task\n");
    auto* editor = root.get();
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    view.Controls().SetRoot(std::move(root));
    view.Controls().SetFocusControl(editor, false);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "task allocation fixture preparation");
    const auto rects = editor->TryGetTextInputRangeRects(view.Controls(), 2u, 5u);
    Check(rects && ! rects->empty(), "task allocation fixture has visible checkbox geometry");
    const auto rect = rects->front();
    const D2D1_POINT_2F point{(rect.left + rect.right) * .5f, (rect.top + rect.bottom) * .5f};
    if (fatalChild)
    {
        RichTaskPointerFaultOrigin(*editor, view, point);
        return;
    }
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-task-pointer-allocation"), "task pointer allocation terminates in its owned child");
    const bool handled = editor->OnMouseDown(view.Controls(), point, false, 0u);
    Check(handled && view.Controls().GetCapturedControl() == editor, "task pointer press is handled and captures the pointer");
    view.Controls().ReleaseMouseCapture();
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
static void TestRetainedRichTextSharedViewsAndResources(GraphicsFixture& gpu, bool semanticRendering = false)
{
    using namespace DxUi;
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "rich-text shared WARP pool");
    EmbeddedHost first, second;
    size_t requests = 0;
    Hr(first.Attach(graphics, {&requests, [](void* context) noexcept { ++*static_cast<size_t*>(context); }}), "rich-text first view");
    Hr(second.Attach(graphics), "rich-text second view");
    std::wstring source = L"# Owned Unicode\r\n**bold** \u03b2 \U0001f600 a\u0301\n\u05d0\u05d1\u05d2\nlast";
    if (semanticRendering)
        source = L"9. [link](local)\n10. <b>native</b>\n\n<p><a href='#x'>HTML</a><br/>&fjlig;</p>\n\n" + source +
                 L"\n\n>\r\n- \r\n\n| a | b |\n| - | - |\n| value |\n";
    auto a       = std::make_unique<RichTextEditor>(source);
    auto b       = std::make_unique<RichTextEditor>(source);
    auto* editor = a.get();
    auto* other  = b.get();
    if (semanticRendering)
    {
        a->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
        b->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
        a->SetMarkdownRenderingEnabled(true);
        b->SetMarkdownRenderingEnabled(true);
    }
    first.Controls().SetRoot(std::move(a));
    second.Controls().SetRoot(std::move(b));
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "rich-text first preparation");
    Hr(PrepareRichEmbeddedContent(second, 320, 160, 144), "rich-text independent density preparation");
    Check(first.Controls().GetHwnd() == nullptr && second.Controls().GetHwnd() == nullptr, "retained rich text creates no embedded HWND");
    const auto prepared        = editor->DebugGetState();
    const auto otherGeneration = other->DebugGetState().layoutGeneration;
    Check(prepared.layoutReady && other->DebugGetState().layoutReady, "both layouts prepared through host DWrite");
    Check(! semanticRendering || (editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic &&
                                  other->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic),
          "semantic resource fixture uses the opt-in geometry engine in both independent views");
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

[[noreturn]] __declspec(noinline) static void EmbeddedRichPaintThrowOrigin(const std::runtime_error& error)
{
    embeddedCallbackOriginMarker.store(6u, std::memory_order_relaxed);
    throw error;
}
static void TestRetainedRichTextFailedPreparation(GraphicsFixture& gpu, bool fatalChild = false)
{
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
    class FailingEditor final : public DxUi::RichTextEditor
    {
    public:
        FailingEditor() : RichTextEditor(L"# Retained source\n**kept**")
        {
        }
        mutable bool fail = true;
        __declspec(noinline) void Paint(DxUi::ControlHost& host) const override
        {
            RichTextEditor::Paint(host);
            if (fail)
            {
                const std::runtime_error error("injected post-raster preparation failure");
                EmbeddedRichPaintThrowOrigin(error);
            }
        }
    };
#pragma warning(pop)
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "failed rich-text preparation pool");
    DxUi::EmbeddedHost view;
    Hr(view.Attach(graphics), "failed rich-text preparation host");
    auto owned   = std::make_unique<FailingEditor>();
    auto* editor = owned.get();
    view.Controls().SetRoot(std::move(owned));
    const auto source = editor->Snapshot();
    if (fatalChild)
    {
        (void)PrepareRichEmbeddedContent(view, 480, 240);
        return;
    }
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-rich-paint-callback-throw"), "rich-text paint callback exception terminates in its owned child");
    editor->fail = false;
    Hr(PrepareRichEmbeddedContent(view, 480, 240), "rich-text preparation succeeds");
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "rich-text surface composes");
    Check(editor->GetText() == source.Text(), "preparation preserves the logical document");
}
