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
}
void TestRichSemanticNativeObjectSharedContract()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::NativeObjectSourceContract(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare shared native object contract"); });
    });
}
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
                std::abs((isolatedStart->bottom - isolatedStart->top) - text.height) < .1f && isolatedEnd->bottom > isolatedEnd->top,
            "an isolated formula without a neighboring text glyph retains usable caret geometry");
    std::cout << "isolated math caret start=" << isolatedStart->top << "," << isolatedStart->bottom << " end=" << isolatedEnd->top << "," << isolatedEnd->bottom
              << '\n';
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
