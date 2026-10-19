#include "Scope.Tests.RichSemantic.h"
#include "../Support/Scope.Tests.RichSemantic.h"
#include "../Support/Scope.Tests.RichSemanticEditor.h"
#include "Controls.Tests.DxUiTestHelpers.h"
namespace
{
void TestRichSemanticCoordinateOracles()
{
    RichSemanticTests::Run(RichSemanticTests::CoordinateOracles);
}
void TestRichSemanticUnicodeAndAdmission()
{
    RichSemanticTests::Run(RichSemanticTests::UnicodeAndAdmission);
}
void TestRichSemanticTransactionOracles()
{
    RichSemanticTests::Run(RichSemanticTests::TransactionOracles);
}
void TestRichSemanticTransactionRejections()
{
    RichSemanticTests::Run(RichSemanticTests::TransactionRejections);
}
void TestRichSemanticNativeRetainedTransactions()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&] {
        RichSemanticEditorTests::Transactions(window.Host(),
                                              [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native source transaction"); });
    });
}
void TestRichMarkdownStructuralListsAndHeadings()
{
    RichSemanticTests::Run(RichCommandTests::ListsAndHeadings);
}
void TestRichMarkdownStructuralInlineTasksLinks()
{
    RichSemanticTests::Run(RichCommandTests::InlineTasksLinks);
}
void TestRichMarkdownStructuralTables()
{
    RichSemanticTests::Run(RichCommandTests::Tables);
}
void TestRichMarkdownStructuralGuardsAndDeletion()
{
    RichSemanticTests::Run(RichCommandTests::GuardsAndDeletion);
}
void TestRichMarkdownNativeStructuralCommands()
{
    AttachedHostWindow window;
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::StructuralCommands(window.Host(),
                                                    [&] { Require(PrepareNativeContentForTests(window.Host()), "prepare native structural command"); });
    });
}
} // namespace
void RunRichSemanticTests()
{
    DXUI_RUN_TEST(TestRichSemanticCoordinateOracles);
    DXUI_RUN_TEST(TestRichSemanticUnicodeAndAdmission);
    DXUI_RUN_TEST(TestRichSemanticTransactionOracles);
    DXUI_RUN_TEST(TestRichSemanticTransactionRejections);
    DXUI_RUN_TEST(TestRichSemanticNativeRetainedTransactions);
    DXUI_RUN_TEST(TestRichMarkdownStructuralListsAndHeadings);
    DXUI_RUN_TEST(TestRichMarkdownStructuralInlineTasksLinks);
    DXUI_RUN_TEST(TestRichMarkdownStructuralTables);
    DXUI_RUN_TEST(TestRichMarkdownStructuralGuardsAndDeletion);
    DXUI_RUN_TEST(TestRichMarkdownNativeStructuralCommands);
}
