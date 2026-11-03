#include "Controls.Tests.RichSemanticGeometry.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls/DxUi.SemanticExtensions.h"
#include "Support.Tests.PreparedFlowchart.h"
#include "Support.Tests.RichSemanticEditor.h"
#include "Support.Tests.RichSemanticGeometry.h"
namespace
{
void TestRichSemanticNativeObjectFailureContainment()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::NativeObjectFailureContainment(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native local object fallback"); });
    });
}
void TestRichSemanticNativeFallbackSyntaxOwnership()
{
    using namespace DxUi::RichText;
    const auto syntax = RichSemanticDocumentTests::Parse(L"# Heading\n\n$x$ **after**\n", MarkdownProfile::RichMarkdown);
    SemanticBuilder builder;
    builder.Begin(syntax);
    const auto original = RichSemanticDocumentTests::Complete(builder, *syntax);
    const auto math     = std::ranges::find(syntax->Inlines(), MarkdownInlineKind::Math, &MarkdownInline::kind);
    Require(math != syntax->Inlines().end(), "fallback ownership fixture contains one native object");
    for (const auto bad : {MarkdownRange{0u, 1u}, MarkdownRange{math->source.first + 1u, math->source.last}, MarkdownRange{3u, 2u}})
    {
        const std::array ranges{bad};
        RichSemanticTests::Reject<std::invalid_argument>([&] { Detail::SemanticExtensionAccess::BeginWithNativeFallbacks(builder, syntax, ranges); },
                                                         "fallback cannot mask arbitrary or partial source");
        Require(builder.Published() == original && ! builder.HasPendingPreparation(), "invalid fallback keeps the prior coherent publication");
    }
    const std::array ranges{math->source};
    Detail::SemanticExtensionAccess::BeginWithNativeFallbacks(builder, syntax, ranges);
    const auto fallback = RichSemanticDocumentTests::Complete(builder, *syntax);
    Require(fallback->Map().Display() == L"Heading\n$x$ after\n" && fallback->Diagnostics().size() == 1u &&
                original->Map().Display().find(L'\ufffc') != std::wstring::npos,
            "source fallback preserves one exact object while old native publications remain immutable");
    builder.Begin(syntax);
    const auto retry = RichSemanticDocumentTests::Complete(builder, *syntax);
    Require(retry->Diagnostics().empty() && retry->Map().Display() == original->Map().Display(),
            "clearing presentation rejection retries the unchanged syntax");
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::RichMarkdown})
        for (const std::wstring source : {L"<p>x<sup>word</sup></p>\n", L"x<sub>word</sub>\n"})
        {
            const auto htmlSyntax = RichSemanticDocumentTests::Parse(source, profile);
            builder.Begin(htmlSyntax);
            const auto htmlOriginal = RichSemanticDocumentTests::Complete(builder, *htmlSyntax);
            const auto owner        = std::ranges::find_if(htmlOriginal->Elements(), [](const auto& element) { return element.nativeHtml; });
            Require(owner != htmlOriginal->Elements().end(), "HTML fallback fixture has an admitted native owner");
            const std::array htmlRanges{owner->source};
            Detail::SemanticExtensionAccess::BeginWithNativeFallbacks(builder, htmlSyntax, htmlRanges);
            const auto htmlFallback = RichSemanticDocumentTests::Complete(builder, *htmlSyntax);
            Require(htmlFallback->Map().Source() == source && htmlFallback->Map().Display().find(source) != std::wstring::npos &&
                        htmlFallback->Diagnostics().empty(),
                    "exact HTML leaf falls back without a diagram diagnostic");
            builder.Begin(htmlSyntax);
            const auto htmlRetry = RichSemanticDocumentTests::Complete(builder, *htmlSyntax);
            Require(htmlRetry->Map().Display() == htmlOriginal->Map().Display(), "clearing the target rejection retries native HTML");
        }
    const std::wstring markdownScript = L"a ^script^ b\n\nother text\n";
    const auto scriptSyntax           = RichSemanticDocumentTests::Parse(markdownScript, MarkdownProfile::RichMarkdown);
    builder.Begin(scriptSyntax);
    const auto scriptOriginal = RichSemanticDocumentTests::Complete(builder, *scriptSyntax);
    const auto scriptOwner    = std::ranges::find_if(scriptOriginal->Elements(), [&](const auto& element) {
        return element.role == SemanticRole::Paragraph && scriptSyntax->Slice(element.source).find(L"^script^") != std::wstring_view::npos;
    });
    Require(scriptOwner != scriptOriginal->Elements().end(), "Markdown script has an exact owning paragraph");
    const std::array scriptRanges{scriptOwner->source};
    Detail::SemanticExtensionAccess::BeginWithNativeFallbacks(builder, scriptSyntax, scriptRanges);
    const auto scriptFallback = RichSemanticDocumentTests::Complete(builder, *scriptSyntax);
    Require(scriptFallback->Map().Display().find(L"a ^script^ b") != std::wstring_view::npos &&
                scriptFallback->Map().Display().find(L"other text") != std::wstring_view::npos,
            "rejected Markdown script falls back only its exact source leaf");
    builder.Begin(scriptSyntax);
    Require(RichSemanticDocumentTests::Complete(builder, *scriptSyntax)->Map().Display() == scriptOriginal->Map().Display(),
            "Markdown script target replacement retries the original semantic leaf");
    const std::wstring fenceSource = L"```math\n\\frac{1}{2}\n```\n";
    const auto fenceSyntax         = RichSemanticDocumentTests::Parse(fenceSource, MarkdownProfile::RichMarkdown);
    builder.Begin(fenceSyntax);
    const auto fenceOriginal = RichSemanticDocumentTests::Complete(builder, *fenceSyntax);
    const auto formula       = std::ranges::find(fenceOriginal->Elements(), SemanticRole::Math, &SemanticElement::role);
    Require(formula != fenceOriginal->Elements().end(), "math fence owns a source-backed native formula");
    const std::array fenceRanges{formula->source};
    Detail::SemanticExtensionAccess::BeginWithNativeFallbacks(builder, fenceSyntax, fenceRanges);
    const auto fenceFallback = RichSemanticDocumentTests::Complete(builder, *fenceSyntax);
    Require(fenceFallback->Map().Source() == fenceSource && fenceFallback->Map().Display().find(fenceSource) != std::wstring::npos &&
                fenceFallback->Diagnostics().size() == 1u,
            "native math fence failure falls back to the complete exact source");
}
void TestRichSemanticNativeObjectSharedContract()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::TransientPreviewRetainsChunks(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native transient preview"); });
        RichSemanticEditorTests::PendingPointerPreservesFocus(window.Host(),
                                                              [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native pending pointer"); });
        RichSemanticEditorTests::ReusedEditorGeometryMatchesFresh(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native geometry parity"); });
        RichSemanticEditorTests::OverCapacityPaddedCellTyping(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native over-capacity table"); });
        RichSemanticEditorTests::ImagePreparationWorkMetric(window.Host(),
                                                            [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native image work metric"); });
        RichSemanticEditorTests::NativeObjectSourceContract(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare shared native object contract"); });
    });
}
#if DXUI_ENABLE_DIAGNOSTICS
void TestRichSemanticNativeTitleTooltips()
{
    AttachedHostWindow window;
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::SelectionCancellationPublication(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native selection cancellation"); });
        RichSemanticEditorTests::TitleTooltipContract(window.Host(),
                                                      [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native title tooltip"); },
                                                      [&](D2D1_POINT_2F point, UINT modifiers)
        {
            const auto scale = static_cast<float>(window.Host().GetDpi()) / 96.f;
            SendMessageW(window.Hwnd(),
                         WM_MOUSEMOVE,
                         modifiers,
                         MAKELPARAM(static_cast<int>(std::lround(point.x * scale)), static_cast<int>(std::lround(point.y * scale))));
        },
                                                      [&](bool down) { SendMessageW(window.Hwnd(), down ? WM_KEYDOWN : WM_KEYUP, VK_CONTROL, 0); });
    });
}
#endif
void TestRichSemanticNativeMathCaretMetrics()
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    AttachedHostWindow window;
    auto root    = std::make_unique<RichTextEditor>(L"Before $\\frac{1}{2}$ after");
    auto* editor = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    window.Host().SetRoot(std::move(root));
    auto* format  = window.Host().GetTextFormat(FontRole::Body);
    auto* factory = window.Host().GetWriteFactory();
    Require(format && factory, "body format and DirectWrite factory are available");
    wil::com_ptr<IDWriteTextLayout> plain;
    THROW_IF_FAILED(factory->CreateTextLayout(L"Before", 6u, format, 400.f, 400.f, plain.put()));
    DWRITE_LINE_METRICS line{};
    UINT32 lineCount{};
    THROW_IF_FAILED(plain->GetLineMetrics(&line, 1u, &lineCount));
    float x{}, y{};
    DWRITE_HIT_TEST_METRICS text{};
    THROW_IF_FAILED(plain->HitTestTextPosition(3u, FALSE, &x, &y, &text));
    Require(lineCount == 1u && text.height > 0.f && line.baseline > 0.f, "standalone SDK body-line oracle has a baseline and text caret");
    for (const UINT dpi : {96u, 144u, 192u})
    {
        bool handled{};
        static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_DPICHANGED, MAKEWPARAM(dpi, dpi), 0, handled));
        Require(handled && PrepareNativeContentForTests(window.Host()), "prepare equation caret at requested DPI");
        const auto adjacent = editor->TryGetTextInputCaretRect(window.Host(), 3u);
        const auto edge     = editor->TryGetTextInputCaretRect(window.Host(), 7u);
        Require(adjacent && edge && std::abs((adjacent->bottom - adjacent->top) - text.height) < .1f &&
                    std::abs((edge->bottom - edge->top) - text.height) < .1f && std::abs(adjacent->top - edge->top) < .1f,
                "formula-edge caret matches adjacent body text and the independent SDK line height at every DPI");
        std::cout << "math caret dpi=" << dpi << " textTop=" << adjacent->top << " edgeTop=" << edge->top << " height=" << text.height
                  << " standaloneBaseline=" << line.baseline << '\n';
    }
    const std::wstring isolated = L"$\\frac{1}{2}$";
    editor->SetText(isolated);
    Require(PrepareNativeContentForTests(window.Host()), "prepare isolated formula caret");
    const auto isolatedStart = editor->TryGetTextInputCaretRect(window.Host(), 0u);
    const auto isolatedEnd   = editor->TryGetTextInputCaretRect(window.Host(), isolated.size());
    Require(isolatedStart && isolatedEnd && std::isfinite(isolatedStart->top) && std::isfinite(isolatedEnd->top) &&
                std::abs((isolatedStart->bottom - isolatedStart->top) - text.height) < .1f &&
                std::abs((isolatedEnd->bottom - isolatedEnd->top) - text.height) < .1f && std::abs(isolatedStart->top - isolatedEnd->top) < .1f,
            "an isolated formula without a neighboring text glyph retains usable caret geometry");
    std::cout << "isolated math caret start=" << isolatedStart->top << "," << isolatedStart->bottom << " end=" << isolatedEnd->top << "," << isolatedEnd->bottom
              << '\n';
    auto styled                   = editor->GetAppearance();
    styled.codeFontSizeDip        = 32.f;
    styled.headingFontSizeDip[0u] = 48.f;
    editor->SetAppearance(styled);
    for (const bool code : {false, true})
    {
        const std::wstring value = code ? L"<code>$x$</code>" : L"# $x$";
        editor->SetText(value);
        Require(PrepareNativeContentForTests(window.Host()), "prepare isolated styled equation caret");
        auto* textFormat = window.Host().GetTextFormat(code ? FontRole::Monospace : FontRole::Body);
        wil::com_ptr<IDWriteTextLayout> styledOracle;
        THROW_IF_FAILED(factory->CreateTextLayout(L"x", 1u, textFormat, 400.f, 400.f, styledOracle.put()));
        THROW_IF_FAILED(styledOracle->SetFontSize(code ? 32.f : 48.f, {0u, 1u}));
        if (! code)
            THROW_IF_FAILED(styledOracle->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, {0u, 1u}));
        DWRITE_HIT_TEST_METRICS hit{};
        THROW_IF_FAILED(styledOracle->HitTestTextPosition(0u, FALSE, &x, &y, &hit));
        const auto sourceCaret = editor->TryGetTextInputCaretRect(window.Host(), value.find(L'$'));
        Require(sourceCaret && std::abs((sourceCaret->bottom - sourceCaret->top) - hit.height) < .1f,
                "isolated formula caret uses the enclosing code or heading font metrics");
    }
    editor->SetAppearance(RichTextEditorAppearance{});
    auto appearance                = editor->GetAppearance();
    appearance.maximumTextWidthDip = 80.f;
    editor->SetAppearance(appearance);
    const std::wstring wrapped = L"prefix words before $\\frac{1}{2}$ followed by more words";
    editor->SetText(wrapped);
    Require(PrepareNativeContentForTests(window.Host()), "prepare wrapped formula caret");
    const auto mathStart    = wrapped.find(L'$');
    const auto mathEnd      = wrapped.find(L'$', mathStart + 1u) + 1u;
    const auto wrappedStart = editor->TryGetTextInputCaretRect(window.Host(), mathStart);
    const auto wrappedEnd   = editor->TryGetTextInputCaretRect(window.Host(), mathEnd);
    const auto after        = editor->TryGetTextInputCaretRect(window.Host(), mathEnd + 1u);
    Require(wrappedStart && wrappedEnd && after && wrappedStart->bottom > wrappedStart->top && wrappedEnd->bottom > wrappedEnd->top &&
                after->bottom > after->top && editor->GetText() == wrapped,
            "wrapped formula edges and following text retain valid source caret geometry");
    std::cout << "wrapped math caret start=" << wrappedStart->top << "," << wrappedStart->bottom << " end=" << wrappedEnd->top << "," << wrappedEnd->bottom
              << " following=" << after->top << "," << after->bottom << '\n';
}
void TestRichSemanticNativeMathAndBroaderDiagrams()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::MathAndBroaderDiagrams(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare in-document native math and diagrams"); });
        RichSemanticEditorTests::InlineMathSourceEditing(window.Host(),
                                                         [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare inline math source session"); });
        for (const UINT dpi : {96u, 144u, 192u})
        {
            bool handled{};
            static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_DPICHANGED, MAKEWPARAM(dpi, dpi), 0, handled));
            Require(handled && window.Host().GetDpi() == static_cast<float>(dpi), "owned native fixture adopts the requested DPI without activation");
            // Element bounds are viewport-clipped. Keep every styled line visible
            // at each DPI so the height oracle measures complete objects.
            const auto width = static_cast<int>(640u * dpi / 96u), height = static_cast<int>(480u * dpi / 96u);
            window.AllowOuterSizeBeyondDesktop({width, height});
            Require(SetWindowPos(window.Hwnd(), nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
                    "size the styled math viewport in DIPs without activation");
            RichSemanticEditorTests::MathRunFontSize(window.Host(),
                                                     [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare styled math at the requested DPI"); });
        }
    });
}
void TestRichSemanticGeometryParagraphsTablesAndSourceRanges()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::ParagraphsTablesAndSourceRanges);
}
void TestRichSemanticGeometryMarkdownSubtlePresentation()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::MarkdownSubtlePresentation);
    RichSemanticTests::Run(RichSemanticGeometryTests::PaddedCodePresentation);
}
void TestRichSemanticCodeChromeUsesReadableTextInk()
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto imaging        = CreateWicFactoryForTest();
    const auto drawing        = PreparedFlowchartTests::DrawingFactory();
    const auto write          = RichSemanticGeometryTests::Factory();
    const std::wstring source = L"```cpp\nint value = 1;\n```\n";
    const auto document       = RichSemanticDocumentTests::Build(source);
    const auto code           = std::ranges::find(document->Elements(), SemanticRole::Code, &SemanticElement::role);
    Require(code != document->Elements().end(), "code chrome regression has one real fenced code element");
    PreparedSemanticGeometry geometry;
    LayoutOptions options;
    options.widthDip             = 220.0f;
    options.markdownPresentation = MarkdownPresentationStyle::Subtle;
    geometry.Begin(write.get(), document, options);
    RichSemanticGeometryTests::Complete(geometry);
    const auto frame     = geometry.ElementBounds(static_cast<std::size_t>(code - document->Elements().begin())).value();
    const auto caret     = geometry.SourceCaret(source.find(L"int"));
    constexpr UINT width = 240u, height = 128u, stride = width * 4u;
    for (const bool dark : {false, true})
    {
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        const auto properties = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        wil::com_ptr<ID2D1SolidColorBrush> text, border;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(dark ? D2D1::ColorF::White : D2D1::ColorF::Black), text.put()));
        const float muted = dark ? .25f : .75f;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(muted, muted, muted), border.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(dark ? D2D1::ColorF::Black : D2D1::ColorF::White));
        geometry.Draw(target.get(), text.get(), border.get(), {0.0f, 0.0f}, 0.0f, static_cast<float>(height));
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(stride * height);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
        const auto readablePixels = [&](D2D1_RECT_F region)
        {
            std::size_t count{};
            for (UINT y = 0u; y < height; ++y)
                for (UINT x = 0u; x < width; ++x)
                    if (static_cast<float>(x) >= region.left && static_cast<float>(x) < region.right && static_cast<float>(y) >= region.top &&
                        static_cast<float>(y) < region.bottom)
                    {
                        const auto ink = pixels[y * stride + x * 4u];
                        count += dark ? ink > 128u : ink < 128u;
                    }
            return count;
        };
        Require(readablePixels({frame.left + 2.0f, frame.top + 2.0f, frame.right - 2.0f, caret.y - 1.0f}) > 0u,
                "language badge uses foreground ink independently of the faint decorative border");
        Require(readablePixels({frame.left + 2.0f, caret.y, caret.x - 2.0f, frame.bottom - 2.0f}) > 0u,
                "physical line numbers use readable foreground ink in both light and dark presentation");
    }
}
void TestRichSemanticDocumentHeadingMarginsAndIndentation()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::DocumentHeadingMarginsAndIndentation);
}
void TestNativeSequenceExtensionsRasterAndResources()
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto imaging = CreateWicFactoryForTest();
    const auto drawing = PreparedFlowchartTests::DrawingFactory();
    const auto write   = PreparedFlowchartTests::WriteFactory();
    const auto calls   = Microsoft::WRL::Make<NativeTextTests::CountingWriteFactory>(write.get());
    if (! calls)
        throw std::bad_alloc();
    const auto graph = FlowchartLayoutTests::Graph(L"sequenceDiagram\nactor Author as Writer\nparticipant App as Editor\nautonumber\n"
                                                   L"Note left of Author: Draft\nAuthor->App: Save\nNote over Author,App: Shared\n"
                                                   L"App-->Author: Saved\nNote right of App: Stored\nAuthor->>App: Done\n");
    LayoutOptions options;
    options.fontFamily  = L"Segoe UI";
    options.fontSizeDip = 16.f;
    options.locale      = L"en-US";
    PreparedFlowchart prepared;
    prepared.Begin(calls.Get(), drawing.get(), graph, options, {1u, 1u});
    Require(PreparedFlowchartTests::Complete(prepared, Require, calls.Get()) == 8u && calls->layouts == 8u,
            "each actor, participant, message and note consumes one budgeted SDK layout");
    const auto plan = prepared.Geometry();
    Require(prepared.Ready() && plan && plan->issue == FlowchartLayoutIssue::None && plan->edges.size() == 6u,
            "extended sequence prepares complete paint resources");
    wil::com_ptr<IDWriteTextFormat> plainFormat;
    THROW_IF_FAILED(write->CreateTextFormat(
        L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.f, L"en-US", plainFormat.put()));
    wil::com_ptr<IDWriteTextLayout> plainLabel;
    THROW_IF_FAILED(write->CreateTextLayout(L"Save", 4u, plainFormat.get(), 200.f, 200.f, plainLabel.put()));
    DWRITE_TEXT_METRICS plainMetrics{};
    THROW_IF_FAILED(plainLabel->GetMetrics(&plainMetrics));
    Require(plan->edges[1u].label.width > plainMetrics.widthIncludingTrailingWhitespace + 10.0,
            "numbered label reserves the independently shaped message plus readable prefix width");
    const auto width  = static_cast<UINT>(std::ceil(plan->extent.width));
    const auto height = static_cast<UINT>(std::ceil(plan->extent.height));
    Require(width < 2048u && height < 1024u, "small sequence fixture has bounded raster storage");
    const auto stride = width * 4u;
    for (const bool dark : {false, true})
    {
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        wil::com_ptr<ID2D1RenderTarget> target;
        const auto properties = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f);
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(dark ? D2D1::ColorF::White : D2D1::ColorF::Black), text.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Blue), stroke.put()));
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(dark ? D2D1::ColorF::Black : D2D1::ColorF::White), fill.put()));
        const auto before = calls->layouts;
        for (unsigned frame = 0u; frame < 3u; ++frame)
        {
            target->BeginDraw();
            target->Clear(D2D1::ColorF(dark ? D2D1::ColorF::Black : D2D1::ColorF::White));
            Require(prepared.Draw(target.get(), text.get(), stroke.get(), fill.get(), {}), "warm sequence paint accepts caller-owned targets");
            THROW_IF_FAILED(target->EndDraw());
        }
        Require(calls->layouts == before && ! prepared.Pending(), "warm light/dark frames create no SDK label layouts or preparation work");
        std::vector<BYTE> pixels(stride * height);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data()));
        const auto inkNear = [&](double x, double y, bool blue)
        {
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                {
                    const auto px = static_cast<int>(x) + dx, py = static_cast<int>(y) + dy;
                    if (px < 0 || py < 0 || px >= static_cast<int>(width) || py >= static_cast<int>(height))
                        continue;
                    const auto offset = static_cast<std::size_t>(py) * stride + static_cast<std::size_t>(px) * 4u;
                    if (blue ? pixels[offset] > 150u && pixels[offset + 1u] < 100u && pixels[offset + 2u] < 100u
                             : (dark ? pixels[offset + 2u] >= 128u : pixels[offset + 2u] < 128u && pixels[offset] < 128u))
                        return true;
                }
            return false;
        };
        const auto& actor = plan->nodes[0u].bounds;
        const auto center = actor.x + actor.width / 2.0;
        Require(inkNear(center - 6.0, actor.y + 10.0, false) && inkNear(center - 10.0, actor.y + 21.0, false) && inkNear(center + 9.0, actor.y + 37.0, false),
                "actor head, arms and legs have actual foreground pixels in light and dark presentation");
        for (const auto index : {0u, 2u, 4u})
        {
            const auto& note = plan->edges[index];
            Require(inkNear((note.path[0u].x + note.path[1u].x) / 2.0, note.path[0u].y, true) &&
                        inkNear(note.path[1u].x, (note.path[0u].y + note.path[1u].y) / 2.0, true),
                    "left, spanning and right notes paint their padded frames");
        }
        for (const auto index : {1u, 3u})
        {
            const auto& message = plan->edges[index];
            Require(! message.hasArrow && ! inkNear(message.path[3u].x - (index == 1u ? 5.0 : -5.0), message.path[3u].y + 4.0, true),
                    "plain solid and dotted message endpoints have no painted arrowhead");
        }
        const auto& numbered = plan->edges[1u].label;
        bool prefixInk{};
        for (double y = numbered.y + 2.0; y < numbered.y + numbered.height - 2.0; y += 2.0)
            prefixInk = prefixInk || inkNear(numbered.x + 5.0, y, false);
        Require(prefixInk, "derived message numbering has actual foreground pixels in the reserved prefix region");
    }
    const auto generation = prepared.Generation();
    prepared.Begin(calls.Get(), drawing.get(), graph, options, {1u, 1u});
    Require(! prepared.Pending() && prepared.Generation() == generation && calls->layouts == 8u, "identical extended sequence reuses resources");
    options.fontSizeDip = 20.f;
    prepared.Begin(calls.Get(), drawing.get(), graph, options, {2u, 1u});
    prepared.Advance({1u, 3u});
    prepared.Cancel();
    Require(prepared.Ready() && prepared.Geometry() == plan && prepared.Generation() == generation,
            "cancellation preserves the last complete actor/note publication");
    prepared.Invalidate();
    Require(! prepared.Ready(), "resource invalidation prevents stale sequence drawing");
    prepared.Clear();
    Require(! prepared.Geometry() && ! prepared.Pending(), "clear retires actor, note and number resources");
}
void TestRichSemanticGeometryAtomicNavigationAndCancellation()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::AtomicNavigationAndCancellation);
}
void TestRichSemanticGeometrySourceCaretSites()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::SourceCaretSites);
}
void TestRichSemanticGeometryHangingMarkersLinksAndHtml()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::HangingMarkersLinksAndHtml);
}
void TestRichSemanticGeometryLinkRasterOracle()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::LinkRasterOracle);
    RichSemanticTests::Run(RichSemanticGeometryTests::DiagramPaletteTargetReplacement);
    RichSemanticTests::Run(RichSemanticGeometryTests::DiagramLabelWidthReflow);
    RichSemanticTests::Run(RichSemanticGeometryTests::HtmlBlockAlignment);
    RichSemanticTests::Run([] { RichSemanticGeometryTests::HtmlScripts(); });
#if DXUI_ENABLE_DIAGNOSTICS
    RichSemanticTests::Run(RichSemanticGeometryTests::MathWidthReflowAndTargetReplacement);
#endif
}
void TestRichSemanticGeometryTaskCheckboxAlignment()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::TaskCheckboxAlignment);
}
void TestRichSemanticGeometryLiteralFootnoteBlockCaretSites()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::LiteralFootnoteBlockCaretSites);
}
void TestRichSemanticGeometryReorderedFootnoteCaretSitesAndSourceLines()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::ReorderedFootnoteCaretSitesAndSourceLines);
}
void TestRichSemanticNativeTaskCheckboxAlignment()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::TaskCheckboxAlignment(window.Host(),
                                                       [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native task checkbox alignment"); });
    });
}
void TestRichSemanticNativeSourceCaretSites()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::SourceCaretSites(window.Host(),
                                                  [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native source caret sites"); });
    });
}
void TestRichSemanticNativeHangingMarkersLinksAndHtml()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::HangingMarkersLinksAndHtml(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native hanging markers and HTML"); });
    });
}
void TestRichSemanticNativeRetainedRendering()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    { RichSemanticEditorTests::Rendering(window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native semantic rendering"); }); });
}
void TestRichSemanticNativeRenderingAdmissionFallback()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::RenderingAdmissionFallback(window.Host(),
                                                            [&]
        { Require(PrepareNativeContentForTests(window.Host()), "prepare native semantic admission fallback"); },
                                                            [&](UINT dpi)
        {
            bool handled{};
            static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_DPICHANGED, MAKEWPARAM(dpi, dpi), 0, handled));
            Require(handled && PrepareNativeContentForTests(window.Host()), "prepare native fallback at changed DPI");
        });
    });
}
} // namespace
void RunRichSemanticGeometryTests()
{
#if DXUI_ENABLE_DIAGNOSTICS
    DXUI_RUN_TEST(TestRichSemanticNativeTitleTooltips);
#endif
    DXUI_RUN_TEST(TestRichSemanticNativeObjectFailureContainment);
    DXUI_RUN_TEST(TestRichSemanticNativeFallbackSyntaxOwnership);
    DXUI_RUN_TEST(TestRichSemanticNativeObjectSharedContract);
    DXUI_RUN_TEST(TestRichSemanticNativeMathCaretMetrics);
    DXUI_RUN_TEST(TestRichSemanticNativeMathAndBroaderDiagrams);
    DXUI_RUN_TEST(TestRichSemanticGeometryParagraphsTablesAndSourceRanges);
    DXUI_RUN_TEST(TestRichSemanticGeometryMarkdownSubtlePresentation);
    DXUI_RUN_TEST(TestRichSemanticCodeChromeUsesReadableTextInk);
    DXUI_RUN_TEST(TestNativeSequenceExtensionsRasterAndResources);
    DXUI_RUN_TEST(TestRichSemanticDocumentHeadingMarginsAndIndentation);
    DXUI_RUN_TEST(TestRichSemanticGeometryAtomicNavigationAndCancellation);
    DXUI_RUN_TEST(TestRichSemanticGeometrySourceCaretSites);
    DXUI_RUN_TEST(TestRichSemanticGeometryHangingMarkersLinksAndHtml);
    DXUI_RUN_TEST(TestRichSemanticGeometryLinkRasterOracle);
    DXUI_RUN_TEST(TestRichSemanticGeometryTaskCheckboxAlignment);
    DXUI_RUN_TEST(TestRichSemanticGeometryLiteralFootnoteBlockCaretSites);
    DXUI_RUN_TEST(TestRichSemanticGeometryReorderedFootnoteCaretSitesAndSourceLines);
    DXUI_RUN_TEST(TestRichSemanticNativeTaskCheckboxAlignment);
    DXUI_RUN_TEST(TestRichSemanticNativeSourceCaretSites);
    DXUI_RUN_TEST(TestRichSemanticNativeHangingMarkersLinksAndHtml);
    DXUI_RUN_TEST(TestRichSemanticNativeRetainedRendering);
    DXUI_RUN_TEST(TestRichSemanticNativeRenderingAdmissionFallback);
}
