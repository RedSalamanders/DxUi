#include "Scope.Tests.RichSemanticDocument.h"
#include "../Support/Scope.Tests.Footnotes.h"
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
    RichSemanticTests::Run(RichSemanticDocumentTests::FootnoteHtmlActionAdmission);
}
void TestRichSemanticDocumentFrontMatterLiteralFallback()
{
    using namespace DxUi::RichText;
    const auto document = RichSemanticDocumentTests::Build(L"\ufeff---\r\n# literal **&amp;**\r\n---\r\n# After\r\n", MarkdownProfile::RichMarkdown);
    RichSemanticDocumentTests::CheckGraph(*document);
    Require(document->Map().Display() == L"\ufeff---\n# literal **&amp;**\n---\nAfter\n",
            "unvalidated metadata stays fully source-visible with no Markdown/entity decoding or fence loss");
    const auto elements = document->Elements();
    const auto metadata = std::find_if(elements.begin(), elements.end(), [](const auto& item) { return item.role == SemanticRole::Metadata; });
    Require(metadata != elements.end() && metadata->source.first == 0u && ! metadata->name.empty(),
            "literal metadata fallback retains a named source-bearing semantic element");
    for (const auto* source : {L"---", L"---\r\nx: [\r\n", L"+++\nkey = [\n"})
    {
        const auto incomplete = RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
        RichSemanticDocumentTests::CheckGraph(*incomplete);
        Require(! incomplete->Map().Display().empty() && incomplete->Syntax()->Nodes()[1u].block.source.last == std::wstring_view(source).size(),
                "incomplete metadata remains a visible exact source owner through EOF");
        Require(incomplete->Syntax()->Diagnostics().size() == 1u && ! incomplete->Elements()[1u].title.empty(),
                "incomplete metadata retains a structured source diagnostic and accessible explanation");
    }
}
void TestRichSemanticDocumentFootnoteDefinitionLiteralFallback()
{
    using namespace DxUi::RichText;
    const auto source   = L"[^n]: **first &amp;**\r\n\r\n    second\r\n\r\nAfter\r\n";
    const auto document = RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
    RichSemanticDocumentTests::CheckGraph(*document);
    Require(document->Map().Display() == L"[^n]: **first &amp;**\n\n    second\n\nAfter\n",
            "initial footnote fallback retains literal marker, entity spelling, indentation and blank body lines");
    const auto element = std::find_if(document->Elements().begin(), document->Elements().end(), [&](const auto& value) {
        return value.syntaxNode != MarkdownNoNode && document->Syntax()->Nodes()[value.syntaxNode].block.kind == MarkdownBlockKind::FootnoteDefinition;
    });
    Require(element != document->Elements().end() && element->role == SemanticRole::Code && element->firstChild == MarkdownNoNode &&
                element->name.starts_with(L"[^n]: **first &amp;**"),
            "literal note fallback has one named source owner without rendering its parsed child subtree a second time");
}
void TestRichSemanticDocumentFootnoteFooterAndSourceOwnership()
{
    for (const auto& scenario : FootnoteResolutionTests::scenarios)
    {
        const auto document = RichSemanticDocumentTests::Build(scenario.source, DxUi::RichText::MarkdownProfile::RichMarkdown);
        RichSemanticDocumentTests::CheckGraph(*document);
        FootnoteResolutionTests::CheckSyntax(*document->Syntax(), scenario, Require);
        FootnoteResolutionTests::CheckSemantic(*document, Require);
    }
    const auto ordered =
        RichSemanticDocumentTests::Build(L"[^z]: **Z**\n[^a]: A\n\nmain[^a] and[^z] again[^a].\n", DxUi::RichText::MarkdownProfile::RichMarkdown);
    Require(ordered->Map().Display() == L"main\u00b9 and\u00b2 again\u00b9.\n1. A\n\u21a91 \u21a92\n2. Z\n\u21a91\n",
            "independent footer oracle renders body styles and first-reference order without raw labels or duplicate definitions");
    for (const auto source : {L"- [^n]: note\n\nmain[^n]\n", L"> [^n]: note\n\nmain[^n]\n", L"> - [^n]: note\n\nmain[^n]\n"})
        Require(RichSemanticDocumentTests::Build(source, DxUi::RichText::MarkdownProfile::RichMarkdown)->Map().Display() == L"main\u00b9\n1. note\n\u21a91\n",
                "a moved note cannot inherit a bullet from its original list/quote declaration container");
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
    DXUI_RUN_TEST(TestRichSemanticDocumentFootnoteDefinitionLiteralFallback);
    DXUI_RUN_TEST(TestRichSemanticDocumentFootnoteFooterAndSourceOwnership);
    DXUI_RUN_TEST(TestRichSemanticDocumentFrontMatterLiteralFallback);
}
