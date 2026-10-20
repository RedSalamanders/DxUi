#include "Scope.Tests.RichSemanticDocument.h"
#include "../../src/Controls/DxUi.NativeFlowchart.h"
#include "../Support/Scope.Tests.Footnotes.h"
#include "../Support/Scope.Tests.RichSemanticDocument.h"
#include "../Support/Scope.Tests.SemanticDiagrams.h"
#include "../Support/Scope.Tests.SemanticSourceBlock.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Scope.Tests.MarkdownCorpus.h"
#include <DxUi/RichMath.h>
#include <type_traits>

namespace
{
void TestRichSemanticNativeMathematicsAndBroaderMermaid()
{
    using namespace DxUi::RichText;
    static_assert(! std::is_nothrow_default_constructible_v<MathNode> && ! std::is_nothrow_default_constructible_v<MathExpression>);
    const std::wstring source = L"Before $a_i^2+\\frac{1}{2}$ after.\r\n\r\n"
                                L"$$\\begin{bmatrix}a&b\\\\c&d\\end{bmatrix}$$\r\n\r\n"
                                L"\\(\\sqrt[3]{x}+\\alpha\\)\r\n\r\n`$literal$` and \\$escaped and $5 and $10.\r\n\r\n"
                                L"```tex\n$x$\n```\n";
    const auto document       = RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
    RichSemanticDocumentTests::CheckGraph(*document);
    Require(std::ranges::count(document->Elements(), SemanticRole::Math, &SemanticElement::role) == 3,
            "inline, display and slash math become objects; code, escapes and currency remain literal");
    for (const auto& element : document->Elements())
        if (element.role == SemanticRole::Math)
        {
            Require(element.display.last == element.display.first + 1u && document->Map().Display()[element.display.first] == L'\ufffc',
                    "native math owns one atomic display position");
            Require(! element.name.empty() && element.source.first < element.source.last, "math exposes exact source and accessible notation");
        }
    const auto common = RichSemanticDocumentTests::Build(source, MarkdownProfile::CommonMark0312);
    Require(std::ranges::count(common->Elements(), SemanticRole::Math, &SemanticElement::role) == 0, "CommonMark does not acquire math syntax");
    for (const auto formula : {L"\\frac12",
                               L"x_1^2",
                               L"\\sqrt[3]{x}",
                               L"\\overline{AB}+\\vec{v}",
                               L"\\begin{aligned}a&=b\\\\c&=d\\end{aligned}",
                               L"\\text{speed in m/s}",
                               L"\\int_0^1 x dx",
                               L"\\left( a \\rightarrow b \\right)",
                               L"\\mathbb{R}+\\mathcal{F}"})
        Require(static_cast<bool>(ParseMath(formula)), "documented mathematical notation parses");
    for (const auto formula : {L"\\input{file}", L"\\frac{a}", L"x^2^3", L"\\begin{matrix}a&b\\\\c\\end{matrix}", L"\\begin{matrix}x\\end{cases}"})
        Require(! ParseMath(formula), "malformed, unsafe or unsupported equations reject the complete formula");
    const auto fallback = RichSemanticDocumentTests::Build(L"$\\input{private}$", MarkdownProfile::RichMarkdown);
    Require(fallback->Map().Display().find(L"$\\input{private}$") != std::wstring::npos && ! fallback->Diagnostics().empty(),
            "rejected math retains exact editable notation and diagnostic");
    using namespace DxUi::RichText::Detail;
    for (const auto body : {L"stateDiagram-v2\n[*] --> Ready\nReady --> Active : Begin\nActive --> [*]\n",
                            L"classDiagram\nCreature <|-- Duck\nDuck : +move()\nDuck --> Pond : visits\n"})
    {
        NativeFlowchartParser parser;
        parser.Begin(body, 7);
        while (parser.HasPendingPreparation())
            parser.Advance(37);
        const auto graph = parser.Published();
        Require(graph && graph->outcome == FlowchartOutcome::Prepared && graph->nodes.size() >= 3 && graph->edges.size() >= 2,
                "state and class diagrams publish complete source-owned models");
        const auto semantic = RichSemanticDocumentTests::Build(L"```mermaid\n" + std::wstring(body) + L"```\n", MarkdownProfile::RichMarkdown);
        RichSemanticDocumentTests::CheckGraph(*semantic);
        Require(std::ranges::count(semantic->Elements(), SemanticRole::Diagram, &SemanticElement::role) == 1, "broader models become native diagram objects");
    }
}
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
void TestRichSemanticDiagramSourceProvenanceAndAdmission()
{
    SemanticDiagramTests::Core(Require);
}
void TestRichSemanticDiagramSdkGeometryAndSourceCarets()
{
    SemanticDiagramTests::Geometry(Require);
}
void TestRichSemanticDiagramPositionedRaster()
{
    SemanticDiagramTests::PositionedRaster(Require);
}
void TestRichSemanticScopedBlockSourcePresentation()
{
    SemanticSourceBlockTests::Core(Require);
}
void TestRichSemanticScopedBlockSourceGeometry()
{
    SemanticSourceBlockTests::Geometry(Require);
}
void TestRichSemanticOpaqueSourceRegionGrammarAndCaret()
{
    SemanticSourceBlockTests::SourceRegions(Require);
}
void RunRichSemanticDocumentTests()
{
    DXUI_RUN_TEST(TestRichSemanticNativeMathematicsAndBroaderMermaid);
    DXUI_RUN_TEST(TestRichSemanticOpaqueSourceRegionGrammarAndCaret);
    DXUI_RUN_TEST(TestRichSemanticScopedBlockSourcePresentation);
    DXUI_RUN_TEST(TestRichSemanticScopedBlockSourceGeometry);
    DXUI_RUN_TEST(TestRichSemanticDiagramSourceProvenanceAndAdmission);
    DXUI_RUN_TEST(TestRichSemanticDiagramSdkGeometryAndSourceCarets);
    DXUI_RUN_TEST(TestRichSemanticDiagramPositionedRaster);
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
