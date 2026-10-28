#include "Controls.Tests.RichSemanticGeometry.h"
#include "../../src/Controls/DxUi.SemanticExtensions.h"
#include "../Support/Support.Tests.RichSemanticEditor.h"
#include "../Support/Support.Tests.RichSemanticGeometry.h"
#include "Controls.Tests.DxUiTestHelpers.h"
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
void TestRichSemanticNativeMathAndBroaderDiagrams()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::MathAndBroaderDiagrams(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare in-document native math and diagrams"); });
    });
}
void TestRichSemanticGeometryParagraphsTablesAndSourceRanges()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::ParagraphsTablesAndSourceRanges);
}
void TestRichSemanticGeometryMarkdownSubtlePresentation()
{
    RichSemanticTests::Run(RichSemanticGeometryTests::MarkdownSubtlePresentation);
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
        RichSemanticEditorTests::RenderingAdmissionFallback(
            window.Host(), [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native semantic admission fallback"); });
    });
}
} // namespace
void RunRichSemanticGeometryTests()
{
    DXUI_RUN_TEST(TestRichSemanticNativeObjectFailureContainment);
    DXUI_RUN_TEST(TestRichSemanticNativeFallbackSyntaxOwnership);
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
