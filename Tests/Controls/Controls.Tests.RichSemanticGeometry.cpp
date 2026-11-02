#include "Controls.Tests.RichSemanticGeometry.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls/DxUi.SemanticExtensions.h"
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
void TestRichSemanticDocumentHeadingMarginsAndIndentation()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::DocumentHeadingMarginsAndIndentation);
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
