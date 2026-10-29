#include "Controls.Tests.RichSemantic.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Support.Tests.RichSemantic.h"
#include "Support.Tests.RichSemanticEditor.h"
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
void TestRichMarkdownNativeHostStructuralRouting()
{
    AttachedHostWindow window;
    auto& host         = window.Host();
    const auto message = [&](UINT msg, WPARAM value)
    {
        bool handled = false;
        host.HandleMessage(window.Hwnd(), msg, value, 0, handled);
        return handled;
    };
    RichSemanticTests::Run([&]
    {
        RichSemanticEditorTests::StructuralHostRouting(host,
                                                       [&] { Require(PrepareNativeContentForTests(host), "prepare structural host routing"); },
                                                       [&](UINT key, UINT modifiers)
        {
            if (modifiers & MK_SHIFT)
                message(WM_KEYDOWN, VK_SHIFT);
            if (modifiers & MK_CONTROL)
                message(WM_KEYDOWN, VK_CONTROL);
            const bool handled = message(WM_KEYDOWN, key);
            message(WM_KEYUP, key);
            if (modifiers & MK_CONTROL)
                message(WM_KEYUP, VK_CONTROL);
            if (modifiers & MK_SHIFT)
                message(WM_KEYUP, VK_SHIFT);
            return handled;
        },
                                                       [&](wchar_t value, UINT) { return message(WM_CHAR, value); });
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
    DXUI_RUN_TEST(TestRichMarkdownNativeHostStructuralRouting);
}
