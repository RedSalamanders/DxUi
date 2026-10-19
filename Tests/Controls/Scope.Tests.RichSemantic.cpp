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
} // namespace
void RunRichSemanticTests()
{
    DXUI_RUN_TEST(TestRichSemanticCoordinateOracles);
    DXUI_RUN_TEST(TestRichSemanticUnicodeAndAdmission);
    DXUI_RUN_TEST(TestRichSemanticTransactionOracles);
    DXUI_RUN_TEST(TestRichSemanticTransactionRejections);
    DXUI_RUN_TEST(TestRichSemanticNativeRetainedTransactions);
}
