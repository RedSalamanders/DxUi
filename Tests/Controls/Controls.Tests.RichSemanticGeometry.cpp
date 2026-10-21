#include "Controls.Tests.RichSemanticGeometry.h"
#include "../Support/Support.Tests.RichSemanticEditor.h"
#include "../Support/Support.Tests.RichSemanticGeometry.h"
#include "Controls.Tests.DxUiTestHelpers.h"
namespace
{
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
    DXUI_RUN_TEST(TestRichSemanticNativeMathAndBroaderDiagrams);
    DXUI_RUN_TEST(TestRichSemanticGeometryParagraphsTablesAndSourceRanges);
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
