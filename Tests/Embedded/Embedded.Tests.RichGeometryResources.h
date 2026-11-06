#include "Controls/TextClipboard.h"
#include "Support.Tests.ClipboardFixture.h"
#include "Support.Tests.RichSemanticGeometry.h"
#pragma once
#include "Controls/DxUi.PaintIndex.h"
#include "Controls/DxUi.PreparedMath.h"
#include "Controls/DxUi.RichSourceGuides.h"
#include <DxUi/RichTextLayout.h>
#include <span>
__declspec(noinline) static void RichPaintIndexFaultOrigin(DxUi::RichText::Detail::PaintIndex& index, std::span<const D2D1_RECT_F> replacement)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    index.Build(replacement.size(), [&](size_t i) { return replacement[i]; });
    failAllocationAfter = std::numeric_limits<size_t>::max();
}
static void TestPaintIndexTransactionalBuild(bool fatalChild = false)
{
    using DxUi::RichText::Detail::PaintIndex;
    const std::array<D2D1_RECT_F, 2u> original{{{0.f, 0.f, 10.f, 10.f}, {0.f, 50.f, 10.f, 60.f}}};
    const std::array<D2D1_RECT_F, 1u> replacement{{{0.f, 100.f, 10.f, 110.f}}};
    PaintIndex index;
    index.Build(original.size(), [&](size_t i) { return original[i]; });
    const auto count = [&](float top, float bottom)
    {
        size_t result{};
        index.ForEach(top, bottom, [&](size_t) { ++result; });
        return result;
    };
    Check(count(0.f, 5.f) == 1u && count(10.f, 11.f) == 1u, "paint index includes exact-boundary stroke ink");
    if (fatalChild)
    {
        RichPaintIndexFaultOrigin(index, replacement);
        Check(false, "paint index allocation child must not continue");
    }
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-paint-index-allocation"), "paint index allocation fails in its owned child");
    index.Build(replacement.size(), [&](size_t i) { return replacement[i]; });
    Check(count(0.f, 5.f) == 0u && count(100.f, 105.f) == 1u, "a second index build replaces the prior tree");
    index.Build(0u, [&](size_t i) { return replacement[i]; });
    Check(count(100.f, 105.f) == 0u, "empty index rebuild clears the prior tree");
}
static wil::com_ptr<IDWriteFactory> RichGeometryFactory()
{
    wil::com_ptr<IDWriteFactory> factory;
    Hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())), "rich resource DWrite factory");
    return factory;
}
__declspec(noinline) static void RichMathAllocationOrigin(IDWriteFactory* factory, const std::shared_ptr<const DxUi::RichText::MathExpression>& expression)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    auto result         = std::make_unique<DxUi::RichText::Detail::PreparedMath>(factory, expression, 24.f);
    while (! result->Advance())
    {
    }
    failAllocationAfter = std::numeric_limits<size_t>::max();
}
static void TestMathPreparationAllocationFailures(const wchar_t* source, bool fatalChild = false)
{
    using namespace DxUi::RichText;
    const auto factory    = RichGeometryFactory();
    const auto expression = std::make_shared<const MathExpression>(ParseMath(source));
    Check(static_cast<bool>(*expression), "math allocation fixture has an admitted formula");
    const auto prepare = [&]
    {
        auto result = std::make_unique<Detail::PreparedMath>(factory.get(), expression, 24.f);
        while (! result->Advance())
        {
        }
        return result;
    };
    const auto retained = prepare();
    const auto original = retained->Size();
    std::size_t points{};
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
        const auto measured           = prepare();
        points                        = allocations;
    }
    Check(points > 0u && points < 4096u, "math preparation has a bounded observed allocation schedule");
    if (fatalChild)
    {
        RichMathAllocationOrigin(factory.get(), expression);
        Check(false, "math preparation allocation child must not continue");
    }
    const auto* child = std::wstring_view(source) == L"1_1^1" ? L"embedded-math-kern-allocation" : L"embedded-math-color-allocation";
    Check(DxUiFailureTests::ExpectFatalChild(child), "math preparation allocation fails in its owned scenario child");
    const auto repeated = prepare();
    Check(retained->Size().width == original.width && retained->Size().height == original.height && repeated->Size().width == original.width &&
              repeated->Size().height == original.height,
          "repeated math preparation preserves the held complete formula");
}
static void TestMathKernPreparationAllocationFailures()
{
    TestMathPreparationAllocationFailures(L"1_1^1");
    TestMathPreparationAllocationFailures(L"\\textcolor{white}{\\fcolorbox{blue}{black}{hello}}+\\color{blue}x^2");
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
__declspec(noinline) static void RichLayoutCopyFaultOrigin(DxUi::RichText::PreparedLayout& target, const DxUi::RichText::PreparedLayout& source)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    target              = source;
    failAllocationAfter = std::numeric_limits<size_t>::max();
}
static void TestRichPreparedLayoutValueFailureAtomicity(bool trace = false, bool fatalChild = false)
{
    using namespace DxUi::RichText;
    const auto factory = RichGeometryFactory();
    Document sourceDocument(L"**copied** a\u0301 \U0001f600\nsecond"), targetDocument(L"previous target");
    PreparedLayout source;
    source.Prepare(factory.get(), Project(sourceDocument.Snapshot(), sourceDocument.Revision(), ViewMode::Rich), 310.0f);
    size_t requests = 0u;
    {
        PreparedLayout target;
        target.Prepare(factory.get(), Project(targetDocument.Snapshot(), targetDocument.Revision(), ViewMode::Rich), 310.0f);
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
        target                        = source;
        countAllocations              = false;
        requests                      = allocations;
    }
    Check(requests > 0u && requests < 128u, "layout copy has a finite observed allocation schedule");
    PreparedLayout candidate;
    candidate.Prepare(factory.get(), Project(targetDocument.Snapshot(), targetDocument.Revision(), ViewMode::Rich), 310.0f);
    if (fatalChild)
    {
        RichLayoutCopyFaultOrigin(candidate, source);
        Check(false, "layout copy allocation child must not continue");
    }
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-layout-copy-allocation"), "layout copy allocation fails in its owned child");
    candidate = source;
    Check(candidate.Content().source == sourceDocument.Text() && source.Ready(), "layout copy retains both source and destination values");
    if (trace)
        std::fprintf(stderr, "layout copy measured %zu allocations; fatal allocation uses an isolated child\n", requests);
    PreparedLayout target;
    {
        const auto oldFailure = failAllocationAfter;
        const auto restore    = wil::scope_exit([=] noexcept { failAllocationAfter = oldFailure; });
        failAllocationAfter   = 0u;
        target                = std::move(source);
    }
    Check(target.Ready() && ! source.Ready() && source.Content().text.empty(), "layout move assignment allocates nothing even at failure point zero");
    std::cout << "rich layout copy measuredRequests=" << requests << '\n';
}
__declspec(noinline) static void RichDocumentGeometryFaultOrigin(DxUi::RichText::PreparedDocumentLayout& prepared,
                                                                 IDWriteFactory* factory,
                                                                 const DxUi::RichText::Document& document,
                                                                 const DxUi::RichText::LayoutOptions& options)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    prepared.BeginPreparation(factory, document.Snapshot(), document.Revision(), DxUi::RichText::ViewMode::Rich, options);
    (void)prepared.AdvancePreparation({1u, 8u});
}
static void TestRichDocumentGeometryFailureAtomicity(bool trace = false, bool requireWhole = false, bool fatalChild = false)
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
    if (requireWhole)
        options.fontSizeDip = 0.0001f; // The minimum private prefix font dominates this tiny document origin.
    const auto prepareChanged = [&](PreparedDocumentLayout& prepared)
    {
        prepared.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        for (size_t turn = 0u; turn < 65536u; ++turn)
        {
            const auto advance = prepared.AdvancePreparation({1u, 8u});
            Check(advance.layoutsPrepared <= 1u && advance.blockSteps <= 8u, "fault probe advances obey both cooperative work budgets");
            if (advance.status == LayoutPreparationStatus::Complete)
                return;
        }
        Check(false, "fault probe continuation must terminate");
    };
    if (fatalChild)
    {
        PreparedDocumentLayout prepared;
        prepared.Prepare(factory.get(), before, revision, ViewMode::Rich, options);
        Check(! requireWhole || prepared.UsesWholeGeometry(), "whole-geometry child enters its compatibility path");
        RichDocumentGeometryFaultOrigin(prepared, factory.get(), document, options);
        return;
    }
    size_t requests = 0u;
    {
        PreparedDocumentLayout prepared;
        prepared.Prepare(factory.get(), before, revision, ViewMode::Rich, options);
        Check(! requireWhole || prepared.UsesWholeGeometry(), "whole-geometry failure fixture actually enters its compatibility path");
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
        prepareChanged(prepared);
        countAllocations = false;
        requests         = allocations;
    }
    Check(requests > 0u && requests < 2048u, "chunked geometry has a finite observed C++ allocation schedule");
    PreparedDocumentLayout prepared;
    prepared.Prepare(factory.get(), before, revision, ViewMode::Rich, options);
    Check(DxUiFailureTests::ExpectFatalChild(requireWhole ? L"embedded-document-geometry-whole-allocation" : L"embedded-document-geometry-chunk-allocation"),
          "document geometry allocation terminates in its owned child");
    prepareChanged(prepared);
    Check(prepared.Ready() && prepared.Revision() == document.Revision() && prepared.SourceMatches(document.Text()),
          "geometry preparation publishes the complete current pair");
    if (trace)
        std::cout << "rich document geometry whole=" << requireWhole << " allocationRequests=" << requests << '\n';
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
static void TestEmbeddedSemanticReusePixels(GraphicsFixture& gpu)
{
    using namespace DxUi;
    std::wstring source = L"- one\n- two\n\n| a | b |\n| --- | --- |\n| c | d |\n\n"
                          L"A formula $x^2$ and ![image](example.png).\n\n"
                          L"```mermaid\ngraph LR;A[Draft]-->B[Ready]\n```\n\n```cpp\nint value = 42;\n```\n\n";
    for (unsigned i = 0u; i < 100u; ++i)
        source += L"**Styled** paragraph and Unicode \u03b2.\n\n";
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "semantic reuse pixel graphics");
    EmbeddedHost reused, fresh;
    Hr(reused.Attach(graphics), "attach reused pixel view");
    Hr(fresh.Attach(graphics), "attach fresh pixel view");
    auto first         = std::make_unique<RichTextEditor>(source);
    auto second        = std::make_unique<RichTextEditor>(source);
    auto* firstEditor  = first.get();
    auto* secondEditor = second.get();
    for (auto* editor : {firstEditor, secondEditor})
    {
        editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        auto appearance                 = editor->GetAppearance();
        appearance.markdownPresentation = RichText::MarkdownPresentationStyle::Subtle;
        editor->SetAppearance(appearance);
    }
    reused.Controls().SetRoot(std::move(first));
    fresh.Controls().SetRoot(std::move(second));
    Hr(PrepareRichEmbeddedContent(reused, 480u, 240u), "prepare initial semantic pixels");
    firstEditor->SetSelection({3u, 3u});
    Check(firstEditor->ReplaceSelection(L"x"), "edit semantic pixel source");
    secondEditor->SetText(firstEditor->GetText());
    Hr(PrepareRichEmbeddedContent(reused, 480u, 240u), "prepare reused semantic pixels");
    Hr(PrepareRichEmbeddedContent(fresh, 480u, 240u), "prepare fresh semantic pixels");
    Check(firstEditor->DebugGetState().reusedGeometryChunks > 0u, "pixel comparison exercises actual semantic layout reuse");
    Hr(gpu.Resize(480u, 240u), "resize semantic pixel fixture");
    std::vector<uint8_t> reusedPixels, freshPixels;
    gpu.Bind();
    Hr(reused.Composite(gpu.context.get(), gpu.Viewport()), "composite reused semantic pixels");
    Hr(gpu.Read(reusedPixels), "read reused semantic pixels");
    gpu.Bind();
    Hr(fresh.Composite(gpu.context.get(), gpu.Viewport()), "composite fresh semantic pixels");
    Hr(gpu.Read(freshPixels), "read fresh semantic pixels");
    Check(reusedPixels == freshPixels, "reused semantic layouts match a fresh WARP render byte for byte");
}

__declspec(noinline) static void RichClipboardBitmapFaultOrigin(std::span<const std::byte> dib, DxUi::TextClipboardContent& output)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)DxUi::DecodeClipboardBitmap(dib, false, output);
}
__declspec(noinline) static void RichClipboardHtmlFaultOrigin(std::span<const std::byte> bytes, std::wstring& output)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)DxUi::DecodeClipboardHtml(bytes, output);
}
static void TestHtmlClipboardFragment(unsigned fatalChild = 0u)
{
    using namespace DxUi;
    BITMAPINFOHEADER dibHeader{};
    dibHeader.biSize      = sizeof(dibHeader);
    dibHeader.biWidth     = 2;
    dibHeader.biHeight    = 2;
    dibHeader.biPlanes    = 1u;
    dibHeader.biBitCount  = 32u;
    dibHeader.biSizeImage = 16u;
    std::vector<std::byte> dib(sizeof(dibHeader) + 16u);
    std::memcpy(dib.data(), &dibHeader, sizeof(dibHeader));
    TextClipboardContent decodedBitmap;
    Check(DecodeClipboardBitmap(dib, false, decodedBitmap) == S_OK && decodedBitmap.bitmapDib == dib && ! decodedBitmap.bitmapV5,
          "packed DIB decoder copies a bounded independent bitmap");
    std::size_t bitmapAllocations{};
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
        Check(DecodeClipboardBitmap(dib, false, decodedBitmap) == S_OK, "measure bitmap copy allocation points");
        countAllocations  = false;
        bitmapAllocations = allocations;
    }
    Check(bitmapAllocations > 0u && bitmapAllocations < 32u, "bitmap copy has a bounded allocation schedule");
    if (fatalChild == 1u)
    {
        TextClipboardContent output;
        RichClipboardBitmapFaultOrigin(dib, output);
        return;
    }
    if (fatalChild == 0u)
        Check(DxUiFailureTests::ExpectFatalChild(L"embedded-clipboard-bitmap-allocation"), "clipboard bitmap allocation terminates in its owned child");
    const auto retainedBitmap = decodedBitmap.bitmapDib;
    Check(FAILED(DecodeClipboardBitmap(std::span(dib).first(dib.size() - 1u), false, decodedBitmap)) && decodedBitmap.bitmapDib == retainedBitmap,
          "truncated bitmap rejection preserves the prior output");
    dibHeader.biWidth = 8193;
    std::memcpy(dib.data(), &dibHeader, sizeof(dibHeader));
    Check(FAILED(DecodeClipboardBitmap(dib, false, decodedBitmap)) && decodedBitmap.bitmapDib == retainedBitmap,
          "oversized bitmap dimensions are rejected without publication");
    const std::string body = "<b>hello</b>";
    const auto header = std::format("Version:1.0\r\nStartHTML:{:010}\r\nEndHTML:{:010}\r\nStartFragment:{:010}\r\nEndFragment:{:010}\r\n", 105, 117, 105, 117);
    Check(header.size() == 105u, "CF_HTML test header has stable byte offsets");
    const auto bytes      = header + body;
    std::wstring fragment = L"sentinel";
    Check(DecodeClipboardHtml(std::as_bytes(std::span(bytes.data(), bytes.size())), fragment) == S_OK && fragment == L"<b>hello</b>",
          "CF_HTML decoder honors byte offsets and UTF-8");
    std::string utf8Body = "<b>";
    utf8Body.push_back(static_cast<char>(0xe6));
    utf8Body.push_back(static_cast<char>(0x9d));
    utf8Body.push_back(static_cast<char>(0xb1));
    utf8Body += "</b>";
    const auto utf8End = 105u + utf8Body.size();
    const auto utf8Header =
        std::format("Version:1.0\r\nStartHTML:{:010}\r\nEndHTML:{:010}\r\nStartFragment:{:010}\r\nEndFragment:{:010}\r\n", 105u, utf8End, 105u, utf8End);
    const auto utf8Bytes = utf8Header + utf8Body;
    Check(DecodeClipboardHtml(std::as_bytes(std::span(utf8Bytes.data(), utf8Bytes.size())), fragment) == S_OK && fragment == L"<b>\u6771</b>",
          "CF_HTML byte offsets preserve a multibyte UTF-8 fragment");
    auto invalid                 = bytes;
    invalid[invalid.size() - 1u] = static_cast<char>(0xff);
    fragment                     = L"sentinel";
    Check(FAILED(DecodeClipboardHtml(std::as_bytes(std::span(invalid.data(), invalid.size())), fragment)) && fragment == L"sentinel",
          "invalid UTF-8 does not publish a fragment");
    if (fatalChild == 2u)
    {
        RichClipboardHtmlFaultOrigin(std::as_bytes(std::span(bytes.data(), bytes.size())), fragment);
        return;
    }
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-clipboard-html-allocation"), "clipboard HTML allocation terminates in its owned child");
    auto owner = reinterpret_cast<HWND>(1);
    ClipboardTransportFixture transport(owner, Check);
    transport.htmlData.reset(GlobalAlloc(GMEM_MOVEABLE, bytes.size()));
    Check(bool(transport.htmlData), "allocate private HTML clipboard medium");
    auto* memory = GlobalLock(transport.htmlData.get());
    Check(memory != nullptr, "lock private HTML clipboard medium");
    {
        const auto unlock = wil::scope_exit([&]() noexcept { GlobalUnlock(transport.htmlData.get()); });
        std::memcpy(memory, bytes.data(), bytes.size());
    }
    TextClipboardContent content;
    Check(ReadTextClipboardContent(SystemTextClipboard(), owner, content, true, true) == S_OK && content.hasHtmlFragment &&
              content.htmlFragment == L"<b>hello</b>" && transport.opens == 1u && transport.closes == 1u,
          "structured clipboard reads HTML through one private ownership attempt");
    transport.fileData.reset(GlobalAlloc(GMEM_MOVEABLE, 1u));
    Check(bool(transport.fileData), "allocate irrelevant private file medium");
    TextClipboardContent htmlOnly;
    Check(ReadTextClipboardContent(SystemTextClipboard(), owner, htmlOnly, false, true) == S_OK && htmlOnly.filePaths.empty() && htmlOnly.hasHtmlFragment &&
              htmlOnly.htmlFragment == L"<b>hello</b>",
          "HTML-only read ignores file media in the same ownership attempt");
    TextClipboardContent oldRead;
    Check(FAILED(ReadTextClipboardContent(SystemTextClipboard(), owner, oldRead, true)) && ! oldRead.hasHtmlFragment,
          "legacy structured read retains file-first failure without requesting HTML");
    invalid = bytes;
    invalid.replace(invalid.find("StartFragment:") + 14u, 10u, "9999999999");
    Check(FAILED(DecodeClipboardHtml(std::as_bytes(std::span(invalid.data(), invalid.size())), fragment)) && fragment == L"sentinel",
          "out-of-range CF_HTML offsets do not publish a fragment");
}
__declspec(noinline) static void RichSourceGuideFaultOrigin(DxUi::RichText::Detail::RichSourceGuides& guides,
                                                            IDWriteFactory* factory,
                                                            const DxUi::RichText::Detail::RichEditorLayout& geometry,
                                                            const DxUi::RichText::Document& source,
                                                            const DxUi::RichText::LayoutOptions& options)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    guides.Begin(source.Snapshot(), 2u, geometry.Generation(), true, true, options, 44.0f);
    HRESULT result = guides.Advance(factory, geometry);
    while (result == S_FALSE)
        result = guides.Advance(factory, geometry);
    Check(result == S_OK, "source guide fatal fixture completes checked native preparation");
}
static void TestSourceGuideAllocationAtomicity(bool fatalChild = false)
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto factory = RichGeometryFactory();
    Document source{L"one \U0001f642 two\tthree\r\nnext line\n"};
    LayoutOptions options;
    options.widthDip = 240.0f;
    RichEditorLayout geometry;
    geometry.BeginPreparation(factory.get(), source.Snapshot(), source.Revision(), ViewMode::Source, options);
    while (geometry.HasPendingPreparation())
        static_cast<void>(geometry.AdvancePreparation());
    const auto finish = [&](RichSourceGuides& guides)
    {
        HRESULT result = guides.Advance(factory.get(), geometry);
        unsigned turns{};
        while (result == S_FALSE)
        {
            Check(++turns <= 128u, "source guides finish within the bounded fixture work budget");
            result = guides.Advance(factory.get(), geometry);
        }
        Check(result == S_OK, "source guides complete checked native preparation");
    };
    const auto seed = [&]
    {
        auto guides = std::make_unique<RichSourceGuides>();
        guides->Begin(source.Snapshot(), 1u, geometry.Generation(), true, true, options, 44.0f);
        finish(*guides);
        return guides;
    };
    const auto stage = [&](RichSourceGuides& guides)
    {
        guides.Begin(source.Snapshot(), 2u, geometry.Generation(), true, true, options, 44.0f);
        finish(guides);
    };
    auto measured          = seed();
    const auto oldCounting = countAllocations;
    const auto oldCalls = allocations, oldBytes = allocationBytes;
    allocations = allocationBytes = 0u;
    countAllocations              = true;
    stage(*measured);
    countAllocations = oldCounting;
    const auto count = allocations;
    allocations      = oldCalls;
    allocationBytes  = oldBytes;
    Check(count > 0u && count < 512u, "source guide preparation has bounded observed allocations");
    auto guides            = seed();
    const auto numberCount = guides->Numbers().size(), markCount = guides->Marks().size();
    if (fatalChild)
    {
        RichSourceGuideFaultOrigin(*guides, factory.get(), geometry, source, options);
        return;
    }
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-source-guide-allocation"), "source guide allocation terminates in its owned child");
    stage(*guides);
    Check(guides->Current(2u, geometry.Generation(), true, true) && guides->Numbers().size() == numberCount && guides->Marks().size() == markCount,
          "guide preparation completes without skipped or duplicated source units");
    Document manyLines{std::wstring(Document::MaximumUnits, L'\n')};
    options.widthDip         = 32.0f;
    options.readingDirection = DWRITE_READING_DIRECTION_RIGHT_TO_LEFT;
    RichEditorLayout largeGeometry;
    largeGeometry.BeginPreparation(factory.get(), manyLines.Snapshot(), manyLines.Revision(), ViewMode::Source, options);
    while (largeGeometry.HasPendingPreparation())
        static_cast<void>(largeGeometry.AdvancePreparation());
    RichSourceGuides windowed;
    for (const auto offset : {std::size_t{0u}, std::size_t{32768u}, std::size_t{65535u}})
    {
        const auto caret   = largeGeometry.SourceCaret(offset);
        const auto bottom  = caret.y + caret.height * 3.0f;
        const auto visible = largeGeometry.VisibleSourceRange(caret.y, bottom);
        const RichSourceGuides::Window window{visible.first, visible.second, caret.y, bottom, -8.0f, 40.0f};
        const auto oldNumbers = windowed.Numbers().size(), oldMarks = windowed.Marks().size();
        windowed.Begin(manyLines.Snapshot(), manyLines.Revision(), largeGeometry.Generation(), true, true, options, 44.0f, window);
        unsigned turns = 0u;
        HRESULT result = windowed.Advance(factory.get(), largeGeometry);
        while (result == S_FALSE)
        {
            Check(windowed.LastWork() <= 256u && windowed.LastLayouts() <= 4u && ++turns < 8u,
                  "maximum-row viewport guides obey independent source/layout turn limits");
            Check(windowed.Numbers().size() == oldNumbers && windowed.Marks().size() == oldMarks,
                  "scroll preparation retains the preceding complete viewport decoration");
            result = windowed.Advance(factory.get(), largeGeometry);
        }
        Check(result == S_OK && windowed.LastWork() <= 256u && windowed.LastLayouts() <= 4u && windowed.Numbers().size() <= 4u && windowed.Marks().size() <= 4u,
              "retained guides depend on the narrow viewport rather than 65537 physical rows");
        Check(! windowed.Numbers().empty() && windowed.Numbers().front().layout->Content().text == std::to_wstring(offset + 1u),
              "deep viewport line numbers use the exact indexed physical source coordinate");
        Check(windowed.HasPublication(largeGeometry.Generation(), true, true) && ! windowed.HasPublication(largeGeometry.Generation() + 1u, true, true),
              "old decoration positions are omitted when replacement geometry makes them unknown");
    }
}
static void TestEmbeddedCosmeticsDoNotWithholdContent(GraphicsFixture& gpu)
{
    using namespace DxUi;
    {
        const auto factory  = RichSemanticGeometryTests::Factory();
        const auto document = RichSemanticDocumentTests::Build(L"before\n\n```cpp\nint value;\n```\n\nafter\n");
        RichText::Detail::PreparedSemanticGeometry geometry;
        RichText::LayoutOptions options;
        options.widthDip = 320.0f;
        geometry.Begin(factory.get(), document, options);
        RichSemanticGeometryTests::Complete(geometry);
        std::vector<std::size_t> visible{999u};
        const auto nan      = std::numeric_limits<float>::quiet_NaN();
        const auto infinity = std::numeric_limits<float>::infinity();
        for (const auto [top, bottom] : {std::pair{nan, 1.0f}, std::pair{0.0f, infinity}, std::pair{2.0f, 1.0f}})
        {
            Check(geometry.TryVisibleCodeElements(top, bottom, visible) == E_INVALIDARG && visible == std::vector<std::size_t>{999u},
                  "invalid code viewport preserves caller output");
        }
        Check(geometry.TryVisibleCodeElements(0.0f, geometry.Height(), visible) == S_OK && ! visible.empty(),
              "valid code viewport succeeds after checked refusals");
        Check(std::ranges::all_of(visible,
                                  [&](size_t index)
        { return index < document->Elements().size() && document->Elements()[index].role == RichText::SemanticRole::Code; }),
              "valid viewport publishes only actual code elements");
    }
    std::wstring source;
    for (unsigned line = 0u; line < 4096u; ++line)
        source += L"alpha\n";
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "cosmetic admission graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "cosmetic admission view");
    auto owned   = std::make_unique<RichTextEditor>(source);
    auto* editor = owned.get();
    editor->SetViewMode(RichText::ViewMode::Source);
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    auto appearance                   = editor->GetAppearance();
    appearance.sourceMonospace        = true;
    appearance.sourceLineNumbers      = true;
    appearance.sourceWhitespaceMarks  = true;
    appearance.colorizeMarkdownSource = true;
    editor->SetAppearance(appearance);
    view.Controls().SetRoot(std::move(owned));
    Hr(gpu.Resize(480u, 240u), "cosmetic admission raster size");
    bool observed = false;
    for (unsigned turn = 0u; turn < 512u; ++turn)
    {
        const auto result = view.Prepare(480u, 240u, 96.0f);
        Hr(result, "cosmetic admission preparation");
        const auto state = editor->DebugGetState();
        if (! state.layoutReady || state.semanticPreparationPending)
            continue;
        observed = true;
        Check(result == S_OK && ! state.preparationPending, "optional source guides never withhold geometrically ready content");
        gpu.Bind();
        Check(view.Composite(gpu.context.get(), gpu.Viewport()) == S_OK, "current content is compositable while optional decorations prepare");
        Check(editor->GetText() == source && ! editor->CanUndo(), "cosmetic preparation preserves exact source and history");
        Check(state.markdownPreparationPending, "source text becomes usable before optional whole-document syntax analysis completes");
        Check(view.NeedsPreparation(), "optional visuals retain an owner-turn request alongside the compositable content frame");
        break;
    }
    Check(observed, "bounded cosmetic admission fixture reaches current text geometry");

    std::wstring blocks;
    for (unsigned index = 0u; index < 48u; ++index)
        blocks += L"```cpp\nblock " + std::to_wstring(index) + L"\n```\n\n";
    auto richOwned = std::make_unique<RichTextEditor>(blocks);
    auto* rich     = richOwned.get();
    rich->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    rich->SetMarkdownRenderingEnabled(true);
    unsigned calls = 0u;
    rich->SetCodeTokenizer([&](std::wstring_view language, std::wstring_view code, std::vector<RichText::CodeToken>& tokens)
    {
        Check(language == L"cpp" && code.starts_with(L"block "), "visible tokenizer receives exact borrowed language and block text");
        ++calls;
        tokens.push_back({{0u, code.size()}, RichText::CodeTokenRole::Keyword});
        return true;
    });
    view.Controls().SetRoot(std::move(richOwned));
    const auto finish = [&]
    {
        unsigned turns = 0u;
        do
        {
            const auto before = calls;
            const auto result = view.Prepare(480u, 240u, 96.0f);
            Hr(result, "viewport code ink preparation");
            const auto state = rich->DebugGetState();
            if (state.layoutReady && ! state.markdownPreparationPending && ! state.semanticPreparationPending)
                Check(result == S_OK && ! state.preparationPending, "code tokenization never withholds current content");
            Check(calls - before <= 1u && ++turns < 2048u, "visible code ink invokes at most one borrowed callback per turn");
        } while (view.NeedsPreparation());
    };
    finish();
    Check(! rich->DebugGetState().codeInkPreparationPending, "all admitted visible code ink finishes through ordinary preparation requests");
    Check(calls > 0u && calls < 16u, "initial code ink excludes offscreen blocks from borrowed callbacks");
    const auto initialCalls = calls;
    for (unsigned turn = 0u; turn < 3u; ++turn)
        Hr(view.Prepare(480u, 240u, 96.0f), "clean cosmetic preparation");
    Check(calls == initialCalls && ! view.NeedsPreparation(), "completed viewport cosmetics do not request idle work");
    Check(rich->SetScrollOffsetDip(rich->DebugGetState().contentHeightDip), "code ink fixture scrolls to another owned viewport");
    finish();
    Check(calls > initialCalls && calls - initialCalls < 16u && rich->GetText() == blocks && ! rich->CanUndo(),
          "scrolling tokenizes the new viewport without whole-document callbacks or source changes");
}
static void TestEmbeddedSourceGuidesPixels(GraphicsFixture& gpu)
{
    using namespace DxUi;
    const std::wstring source = L"alpha  beta\tend\r\nsecond line\n";
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "source guide graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "source guide view");
    auto owned   = std::make_unique<RichTextEditor>(source);
    auto* editor = owned.get();
    editor->SetViewMode(RichText::ViewMode::Source);
    auto appearance              = editor->GetAppearance();
    appearance.sourceMonospace   = true;
    appearance.sourceLineNumbers = true;
    editor->SetAppearance(appearance);
    view.Controls().SetRoot(std::move(owned));
    Hr(gpu.Resize(480u, 240u), "source guide raster size");
    const auto capture = [&]
    {
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare source guide pixels");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "source guide composite");
        std::vector<std::uint8_t> pixels(0u);
        Hr(gpu.Read(pixels), "source guide readback");
        return pixels;
    };
    const auto plain                 = capture();
    appearance.sourceWhitespaceMarks = true;
    editor->SetAppearance(appearance);
    const auto marked = capture();
    Check(plain != marked && editor->GetText() == source && ! editor->CanUndo(),
          "whitespace marks alter rendered pixels without changing counted source or history");
    appearance.sourceLineNumbers = false;
    editor->SetAppearance(appearance);
    const auto noNumbers = capture();
    Check(noNumbers != marked && editor->GetText() == source, "physical line gutter changes rendered pixels without changing source");
    appearance.sourceLineNumbers = true;
    editor->SetAppearance(appearance);
    std::wstring longSource;
    for (unsigned line = 0u; line < 600u; ++line)
        longSource += L"pending \tline\n";
    editor->SetText(longSource);
    Hr(view.Prepare(480u, 240u, 96.0f), "begin long source guide preparation");
    Check(view.NeedsPreparation(), "large guide preparation yields before publication");
    editor->SetText(source);
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "replace pending source guides");
    Check(editor->GetText() == source && ! view.NeedsPreparation(), "new source supersedes pending guides without stale publication");
}
__declspec(noinline) static void RichCodeCopyFaultOrigin(DxUi::RichTextEditor& editor, DxUi::EmbeddedHost& view)
{
    DxUi::Failure::Scope scope;
    failAllocationAfter = 0u;
    (void)editor.OnKeyDown(view.Controls(), 'C', MK_CONTROL | MK_SHIFT);
}
static void TestEmbeddedCodeLineNumberPixels(GraphicsFixture& gpu, bool fatalChild = false)
{
    using namespace DxUi;
    std::wstring source = L"```cpp\nfirst line with many words to force wrapping in the available code width across visual lines\nsecond line\n";
    for (unsigned line = 0u; line < 30u; ++line)
        source += L"remaining line\n";
    source += L"```\n";
    std::shared_ptr<GraphicsDevice> graphics;
    Hr(GraphicsDevice::Create(gpu.device.get(), graphics), "code gutter graphics");
    EmbeddedHost view;
    Hr(view.Attach(graphics), "code gutter view");
    auto owned   = std::make_unique<RichTextEditor>(source);
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    auto appearance                 = editor->GetAppearance();
    appearance.markdownPresentation = RichText::MarkdownPresentationStyle::Subtle;
    editor->SetAppearance(appearance);
    std::wstring copied;
    unsigned copyCalls{};
    editor->SetOnCodeCopyRequested(
        [&](std::wstring_view text)
    {
        copied.assign(text);
        ++copyCalls;
    },
        L"Kopieren");
    view.Controls().SetRoot(std::move(owned));
    view.Controls().SetFocusControl(editor);
    auto site = std::make_shared<TestEmbeddedAccessibilitySite>();
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> uiaRoot;
    wil::com_ptr_nothrow<IInvokeProvider> staleCopy;
    Hr(gpu.Resize(480u, 240u), "code gutter raster size");
    for (const auto direction : {FlowDirection::LeftToRight, FlowDirection::RightToLeft})
    {
        editor->SetFlowDirection(direction);
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare code gutter");
        if (! uiaRoot)
        {
            Check(view.AttachAccessibility(site, 0x434f5059u, {{0, 0, 480, 240}, true}) == S_OK, "attach prepared code copy accessibility");
            Check(view.GetAccessibilityProvider(uiaRoot.put()) == S_OK && bool(uiaRoot), "prepared code copy accessibility root");
            site->root = uiaRoot.get();
        }
        else
            Check(view.UpdateAccessibility({{0, 0, 480, 240}, true}) == S_OK, "publish directional code accessibility");
        const auto document = editor->GetAcknowledgedSemanticDocument(view.Controls());
        Check(static_cast<bool>(document), "code gutter has current semantics");
        const auto element = std::ranges::find(document->Elements(), RichText::SemanticRole::Code, &RichText::SemanticElement::role);
        Check(element != document->Elements().end(), "code gutter owns a code block");
        const auto frame       = editor->GetSemanticElementBounds(view.Controls(), static_cast<std::size_t>(element - document->Elements().begin()));
        const auto caret       = editor->TryGetTextInputCaretRect(view.Controls(), source.find(L"first line"));
        const auto secondCaret = editor->TryGetTextInputCaretRect(view.Controls(), source.find(L"second line"));
        Check(frame && caret && secondCaret && secondCaret->top > caret->bottom + 1.0f,
              "wrapped first physical line and second source line have distinct positions");
        gpu.Bind();
        Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "code gutter composite");
        std::vector<std::uint8_t> pixels(0u);
        Hr(gpu.Read(pixels), "code gutter pixels");
        const auto sample = [&](unsigned x, unsigned y)
        {
            const auto at = (static_cast<std::size_t>(y) * gpu.width + x) * 4u;
            return std::array{pixels[at], pixels[at + 1u], pixels[at + 2u]};
        };
        const auto left       = direction == FlowDirection::LeftToRight ? frame->left + 6.0f : frame->right - 46.0f;
        const auto top        = static_cast<unsigned>(std::max(0.0f, caret->top));
        const auto bottom     = static_cast<unsigned>(std::min(static_cast<float>(gpu.height), caret->bottom));
        const auto firstX     = static_cast<unsigned>(std::max(0.0f, left + 2.0f));
        const auto lastX      = static_cast<unsigned>(std::min(static_cast<float>(gpu.width), left + 36.0f));
        const auto background = sample(firstX, top + 1u);
        unsigned differing{};
        for (auto y = top; y < bottom; ++y)
            for (auto x = firstX; x < lastX; ++x)
                differing += sample(x, y) != background ? 1u : 0u;
        Check(differing > 4u, "prepared physical line number paints in the correct directional gutter");
        const auto secondTop        = static_cast<unsigned>(std::max(0.0f, secondCaret->top));
        const auto secondBottom     = static_cast<unsigned>(std::min(static_cast<float>(gpu.height), secondCaret->bottom));
        const auto secondBackground = sample(firstX, secondTop + 1u);
        unsigned secondDiffering{};
        for (auto y = secondTop; y < secondBottom; ++y)
            for (auto x = firstX; x < lastX; ++x)
                secondDiffering += sample(x, y) != secondBackground ? 1u : 0u;
        Check(secondDiffering > 4u, "second physical line number aligns after wrapped visual lines");
        const auto icon = RichText::Detail::CodeCopyBounds(*frame, direction == FlowDirection::RightToLeft);
        Check(icon.has_value(), "copy action has a hit target in the prepared header");
        const auto point = D2D1::Point2F((icon->left + icon->right) * 0.5f, (icon->top + icon->bottom) * 0.5f);
        wil::com_ptr_nothrow<IRawElementProviderFragment> peer;
        Hr(uiaRoot->ElementProviderFromPoint(point.x, point.y, peer.put()), "hit code copy UIA child");
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        Hr(peer.query_to(simple.put()), "code copy simple provider");
        VARIANT name{};
        Hr(simple->GetPropertyValue(UIA_NamePropertyId, &name), "code copy localized name");
        Check(name.vt == VT_BSTR && std::wstring_view(name.bstrVal, SysStringLen(name.bstrVal)) == L"Kopieren",
              "copy action exposes the consumer-localized name");
        VariantClear(&name);
        wil::com_ptr_nothrow<IUnknown> pattern;
        Hr(simple->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "code copy Invoke pattern");
        Check(bool(pattern), "copy child advertises Invoke");
        wil::com_ptr_nothrow<IInvokeProvider> invoke;
        Hr(pattern.query_to(invoke.put()), "code copy Invoke interface");
        const auto beforeInvoke = copyCalls;
        Hr(invoke->Invoke(), "invoke code copy through UIA");
        Check(copyCalls == beforeInvoke + 1u && copied == element->name, "UIA copy passes counted rendered code to the application");
        wil::com_ptr_nothrow<IRawElementProviderFragment> parent;
        Hr(peer->Navigate(NavigateDirection_Parent, parent.put()), "code copy parent");
        wil::com_ptr_nothrow<IRawElementProviderSimple> parentSimple;
        Hr(parent.query_to(parentSimple.put()), "code semantic parent");
        pattern.reset();
        Hr(parentSimple->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "code source edit remains invokable");
        Check(bool(pattern), "copy child does not replace the source-edit action");
        staleCopy                = invoke;
        const auto before        = editor->GetSelection();
        const auto revision      = editor->Revision();
        const auto previousCalls = copyCalls;
        Check(editor->OnMouseDown(view.Controls(), point, false, 0u) && editor->OnMouseUp(view.Controls(), point, false, 0u),
              "code copy handles a complete pointer gesture");
        Check(copyCalls == previousCalls + 1u && copied == element->name && editor->GetSelection() == before && editor->Revision() == revision,
              "copy intent borrows the rendered code without changing source or selection");
        Check(editor->OnMouseDown(view.Controls(), point, false, 0u), "start cancelable copy gesture");
        editor->OnMouseMove(view.Controls(), D2D1::Point2F(point.x, frame->bottom + 10.0f), 0u);
        editor->OnMouseUp(view.Controls(), point, false, 0u);
        Check(copyCalls == previousCalls + 1u, "leaving the copy target cancels the pointer gesture");
        editor->SetSelection({source.find(L"first line"), source.find(L"first line")});
        Check(editor->OnKeyDown(view.Controls(), 'C', MK_CONTROL | MK_SHIFT) && copyCalls == previousCalls + 2u,
              "Ctrl+Shift+C requests the current code block from the keyboard");
    }
    Check(editor->OnMouseWheel(view.Controls(), {}, -WHEEL_DELTA, 0u), "scroll code copy action offscreen");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare scrolled code accessibility");
    Check(view.UpdateAccessibility({{0, 0, 480, 240}, true}) == S_OK, "publish scrolled code accessibility");
    wil::com_ptr_nothrow<IRawElementProviderSimple> offscreen;
    Hr(staleCopy.query_to(offscreen.put()), "offscreen copy simple provider");
    VARIANT offscreenValue{};
    Hr(offscreen->GetPropertyValue(UIA_IsOffscreenPropertyId, &offscreenValue), "offscreen copy property");
    Check(offscreenValue.vt == VT_BOOL && offscreenValue.boolVal == VARIANT_TRUE, "copy child remains discoverable and reports offscreen");
    VariantClear(&offscreenValue);
    Hr(staleCopy->Invoke(), "offscreen copy remains invokable");
    editor->SetOnCodeCopyRequested({});
    Check(staleCopy->Invoke() == UIA_E_ELEMENTNOTAVAILABLE, "retired copy peer cannot invoke after callback removal");
    editor->SetOnCodeCopyRequested(
        [&](std::wstring_view text)
    {
        copied.assign(text);
        ++copyCalls;
    },
        L"Kopieren");
    for (const auto& [prefix, suffix] : {
             std::pair<std::wstring_view, std::wstring_view>{L"```\r\na", L"b\r\n```\r\n"},
             {L"> ```\r\n> a", L"b\r\n> ```\r\n"},
             {L"- item\r\n\r\n  ```\r\n  a", L"b\r\n  ```\r\n"},
             {L"    a", L"b\r\n"},
         })
    {
        std::wstring counted(prefix);
        const auto caret = counted.size();
        counted.push_back(L'\0');
        counted.append(suffix);
        editor->SetText(counted);
        Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "counted code copy preparation");
        editor->SetSelection({caret, caret});
        Check(editor->OnKeyDown(view.Controls(), 'C', MK_CONTROL | MK_SHIFT) && copied == std::wstring_view(L"a\0b\n", 4u),
              "code copy preserves counted NUL and normalizes CRLF inside fenced, quoted, list and indented blocks");
    }
    unsigned delivered = 0u;
    editor->SetOnCodeCopyRequested([&](std::wstring_view) { ++delivered; }, L"Kopieren");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "prepare code copy allocation sweep");
    const auto beforeRevision  = editor->Revision();
    const auto beforeSelection = editor->GetSelection();
    size_t copyAllocations     = 0u;
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
        Check(editor->OnKeyDown(view.Controls(), 'C', MK_CONTROL | MK_SHIFT), "measure code copy allocations");
        copyAllocations = allocations;
    }
    Check(copyAllocations > 0u && delivered == 1u, "code copy allocation sweep observes a successful callback");
    if (fatalChild)
    {
        RichCodeCopyFaultOrigin(*editor, view);
        return;
    }
    Check(DxUiFailureTests::ExpectFatalChild(L"embedded-code-copy-allocation"), "code copy allocation terminates in its owned child");
    Check(editor->OnKeyDown(view.Controls(), 'C', MK_CONTROL | MK_SHIFT) && delivered == 2u && editor->Revision() == beforeRevision &&
              editor->GetSelection() == beforeSelection,
          "code copy invokes its callback without changing source or selection");
    editor->SetOnCodeCopyRequested([&](std::wstring_view text) { copied.assign(text); }, L"Kopieren");
    editor->SetText(L"```\n```\n");
    Hr(PrepareRichEmbeddedContent(view, 480u, 240u), "empty code copy preparation");
    editor->SetSelection({1u, 1u});
    copied = L"sentinel";
    Check(editor->OnKeyDown(view.Controls(), 'C', MK_CONTROL | MK_SHIFT) && copied.empty(), "empty code copy omits the synthetic caret placeholder");
    site->root = nullptr;
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
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "chunk reuse initial frame");
    Hr(PrepareRichEmbeddedContent(second, 320, 160, 144), "chunk reuse independent variant");
    const auto initial     = editor->DebugGetState();
    const auto independent = other->DebugGetState();
    Check(initial.retainedGeometryChunks > 1u && initial.retainedGeometryChunks <= 128u, "embedded source really uses bounded multiple chunks");
    editor->SetSelection({seed.size() / 2u, seed.size() / 2u + 1u});
    Check(editor->ReplaceSelection(L"x") && ! editor->DebugGetState().layoutReady &&
              editor->DebugGetState().retainedGeometryChunks == initial.retainedGeometryChunks,
          "embedded source invalidation hides stale geometry while retaining its bounded graph");
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "chunk reuse changed frame");
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
    Hr(PrepareRichEmbeddedContent(first, 320, 240), "chunk geometry width variant");
    Check(editor->DebugGetState().reusedGeometryChunks == 0u, "width change cannot reuse incompatible geometry");
    first.SetVisible(false);
    Check(! editor->DebugGetState().layoutReady && editor->DebugGetState().retainedGeometryChunks == 0u && ! first.NeedsPreparation(),
          "hidden embedded view retires every geometry chunk without scheduling work");
    first.SetVisible(true);
    Hr(PrepareRichEmbeddedContent(first, 480, 240), "resumed chunk geometry");
    Check(editor->DebugGetState().reusedGeometryChunks == 0u && editor->Undo() && editor->GetText() == seed,
          "resuming rebuilds resources while retaining independent source history");
}
static void TestRichDocumentGeometryReplacementPeakBudget()
{
    using namespace DxUi::RichText;
    const auto factory = RichGeometryFactory();
    LayoutOptions options;
    options.widthDip = 321.25f;
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line)
        seed += L"line " + std::to_wstring(line) + L" **bold** a\u0301 `code`\r\n";
    const auto position    = seed.find(L"line 512") + 2u;
    const auto oldCounting = countLiveBytes;
    const auto oldBytes    = liveBytes;
    auto* const oldPeak    = liveBytesPeak;
    ptrdiff_t retained = 0, released = 0, peak = 0;
    bool correct = false;
    {
        const auto restore = wil::scope_exit([=] noexcept
        {
            countLiveBytes = oldCounting;
            liveBytes      = oldBytes;
            liveBytesPeak  = oldPeak;
        });
        liveBytes          = 0;
        countLiveBytes     = true;
        liveBytesPeak      = &peak;
        {
            Document document(seed);
            PreparedDocumentLayout prepared;
            prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
            for (size_t edit = 0u; edit < 512u; ++edit)
            {
                document.Replace(position, position + 1u, edit % 2u ? L"x" : L"y", {0u, 0u});
                prepared.Invalidate();
                prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
            }
            retained = liveBytes;
            correct  = prepared.SourceMatches(document.Text()) && prepared.LastPreparation().reusedChunks > 0u && prepared.ChunkCount() <= 128u;
        }
        released       = liveBytes;
        countLiveBytes = false;
    }
    std::cout << "rich geometry replacement lines=1024 edits=512 retainedCppBytes=" << retained << " peakCppBytes=" << peak << " releasedDelta=" << released
              << '\n';
    Check(correct && retained >= 0 && retained <= 8 * 1024 * 1024 && peak >= retained && peak <= 16 * 1024 * 1024 && released == 0,
          "edited source/history and old/staged chunk replacement stay under eight retained/sixteen peak MiB and release all tracked bytes");
}
