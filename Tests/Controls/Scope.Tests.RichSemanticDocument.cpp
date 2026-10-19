#include "Scope.Tests.RichSemanticDocument.h"
#include "../Support/Scope.Tests.RichSemanticDocument.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Scope.Tests.MarkdownCorpus.h"

namespace
{
void TestRichSemanticDocumentStructuralStyleValues()
{
    RichSemanticTests::Run(RichSemanticDocumentTests::StructuralStyleValues);
}
void TestRichSemanticDocumentSourceFormatOwnership()
{
    RichSemanticTests::Run(RichSemanticDocumentTests::SourceFormatOwnership);
}
void TestRichSemanticDocumentDirectedValues()
{
    RichSemanticTests::Run(RichSemanticDocumentTests::DirectedValues);
}
void TestRichSemanticDocumentSchedulingAndAdmission()
{
    RichSemanticTests::Run(RichSemanticDocumentTests::SchedulingAndAdmission);
}
void TestRichSemanticDocumentNativeHtmlAndLinkPresentation()
{
    RichSemanticTests::Run(RichSemanticDocumentTests::NativeHtmlAndLinkPresentation);
}
void TestRichSemanticDocumentPinnedSourceCases()
{
    using namespace DxUi::RichText;
    std::size_t admitted = 0u, rejectedSource = 0u, failures = 0u;
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029})
    {
        const auto cases = profile == MarkdownProfile::CommonMark0312 ? std::span<const std::wstring_view>(MarkdownCorpus::kCommonMark)
                                                                      : std::span<const std::wstring_view>(MarkdownCorpus::kGfm);
        for (std::size_t index = 0u; index < cases.size(); ++index)
        {
            const auto syntax = RichSemanticDocumentTests::Parse(cases[index], profile);
            SemanticBuilder builder;
            if (! IsValidUtf16(cases[index]))
            {
                RichSemanticTests::Reject<std::invalid_argument>([&] { builder.Begin(syntax); }, "raw parser NUL remains outside Document admission");
                ++rejectedSource;
                continue;
            }
            try
            {
                builder.Begin(syntax);
                const auto document = RichSemanticDocumentTests::Complete(builder, *syntax);
                RichSemanticDocumentTests::CheckGraph(*document);
                ++admitted;
            }
            catch (const std::exception& error)
            {
                ++failures;
                std::cerr << "SEMANTIC MAP profile=" << static_cast<unsigned>(profile) << " exampleIndex=" << index << " error=" << error.what() << '\n';
            }
        }
    }
    std::cout << "semantic pinned source cases admitted=" << admitted << " rejectedDocumentSource=" << rejectedSource << " failures=" << failures << '\n';
    Require(admitted + rejectedSource + failures == 1324u && failures == 0u,
            "all pinned source cases construct legal semantic maps or receive explicit existing Document admission rejection");
    // This checks semantic-value reconstruction and coordinates. Native HTML
    // pixels, browser equivalence and physical assistive technology are separate.
}
} // namespace
void RunRichSemanticDocumentTests()
{
    DXUI_RUN_TEST(TestRichSemanticDocumentStructuralStyleValues);
    DXUI_RUN_TEST(TestRichSemanticDocumentSourceFormatOwnership);
    DXUI_RUN_TEST(TestRichSemanticDocumentDirectedValues);
    DXUI_RUN_TEST(TestRichSemanticDocumentNativeHtmlAndLinkPresentation);
    DXUI_RUN_TEST(TestRichSemanticDocumentSchedulingAndAdmission);
    DXUI_RUN_TEST(TestRichSemanticDocumentPinnedSourceCases);
}
