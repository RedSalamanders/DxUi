#include "Controls.Tests.RichSemanticDocument.h"
#include "../../src/Controls/DxUi.NativeFlowchart.h"
#include "../../src/Controls/DxUi.PreparedMath.h"
#include "../Support/Support.Tests.Footnotes.h"
#include "../Support/Support.Tests.MathCorpus.h"
#include "../Support/Support.Tests.RichSemanticDocument.h"
#include "../Support/Support.Tests.SemanticDiagrams.h"
#include "../Support/Support.Tests.SemanticSourceBlock.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.MarkdownCorpus.h"
#include <DxUi/RichMath.h>
#include <iterator>
#include <type_traits>

namespace
{
void TestRichMathTextWhitespaceAndPreparation()
{
    using namespace DxUi::RichText;
    const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\text{speed\r\n in\t {m/s}}"));
    Require(static_cast<bool>(*expression), "text-mode whitespace remains accepted mathematical source");
    Require(expression->root.children.size() == 1u && expression->root.children[0u].text == L"speed in m/s",
            "TeX text groups collapse source whitespace and omit grouping braces before shaping");
    Detail::PreparedMath paint(PreparedFlowchartTests::WriteFactory().get(), expression, 24.f);
    unsigned turns{};
    while (! paint.Advance())
        Require(++turns < 16u, "multiline text-mode source prepares bounded single-line glyphs");
    Require(paint.Size().width > 1.f && paint.Size().height > 1.f, "text-mode whitespace cannot discard surrounding semantic geometry");
    Require(! ParseMath(std::wstring{L'x', wchar_t(1), L'y'}), "unsupported control characters reject before native shaping");
}
void TestRichMathRejectedObjectOwnsItsDiagnostic()
{
    using namespace DxUi::RichText;
    const auto document =
        RichSemanticDocumentTests::Build(L"[before $\\unknown{x}$ after](https://example.test \"original title\")", MarkdownProfile::RichMarkdown);
    RichSemanticDocumentTests::CheckGraph(*document);
    const auto link = std::ranges::find(document->Elements(), SemanticRole::Link, &SemanticElement::role);
    const auto math = std::ranges::find(document->Elements(), SemanticRole::Math, &SemanticElement::role);
    Require(link != document->Elements().end() && link->title == L"original title", "a rejected equation never overwrites its enclosing link metadata");
    Require(math != document->Elements().end() && document->Diagnostics().size() == 1u &&
                document->Diagnostics()[0u].element == static_cast<std::size_t>(math - document->Elements().begin()) &&
                document->Diagnostics()[0u].source == math->source && math->parent == static_cast<std::size_t>(link - document->Elements().begin()),
            "literal math retains its own accessible source owner and exact diagnostic range");
    Require(document->Map().Display() == L"before $\\unknown{x}$ after\n", "rejection affects only the equation and leaves surrounding semantics intact");
}
void TestRichMathLimitModifiersAndSymbolIdentity()
{
    using namespace DxUi::RichText;
    for (const auto source : {L"\\sum\\limits_{i=1}^{n}", L"\\int\\nolimits_0^1"})
    {
        const auto expression = ParseMath(source);
        Require(expression && expression.root.children.size() == 1u && expression.root.children[0u].kind == MathKind::Scripts &&
                    expression.root.children[0u].children[0u].kind == MathKind::Text && ! expression.root.children[0u].children[0u].text.empty(),
                "limits modifiers attach scripts to the operator rather than a synthetic space");
    }
    for (const auto source : {L"\\limits x", L"x\\nolimits_2", L"\\sum\\limits\\nolimits_2"})
        Require(! ParseMath(source), "limit modifiers without one eligible operator reject instead of changing another atom");
    const auto symbols = ParseMath(L"\\epsilon\\varepsilon\\phi\\varphi\\cdot");
    std::wstring text;
    const auto append = [&](auto&& self, const MathNode& node) -> void
    {
        if (node.kind == MathKind::Text)
            text += node.text;
        for (const auto& child : node.children)
            self(self, child);
    };
    append(append, symbols.root);
    Require(symbols && text == L"\u03f5\u03b5\u03d5\u03c6\u22c5", "TeX variant Greek letters and centered dot preserve their distinct identities");
}
void TestRichMathExplicitDelimiterRecognition()
{
    using namespace DxUi::RichText;
    for (const auto source : {L"a \\( x \\)2 b", L"a \\(x \\) b", L"a \\( x\\) b"})
    {
        const auto document = RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
        Require(std::ranges::count(document->Elements(), SemanticRole::Math, &SemanticElement::role) == 1u,
                "explicit slash delimiters permit edge whitespace and a following digit without dollar currency heuristics");
    }
    for (const auto source : {L"$ x$", L"$x $", L"$x$2", L"`\\(x\\)`", L"<span title='\\(x\\)'>literal</span>"})
    {
        const auto document = RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
        Require(std::ranges::count(document->Elements(), SemanticRole::Math, &SemanticElement::role) == 0u,
                "dollar whitespace/currency rules and code/HTML token boundaries retain literal source");
    }
}
void TestRichMathAlignedCasesAndMatrixGeometry()
{
    using namespace DxUi::RichText;
    const auto factory = PreparedFlowchartTests::WriteFactory();
    for (const std::wstring environment : {L"aligned", L"cases", L"matrix"})
    {
        const auto source     = L"\\begin{" + environment + L"}x&=1\\\\xxxx&=1234\\end{" + environment + L"}";
        const auto expression = std::make_shared<const MathExpression>(ParseMath(source));
        Require(static_cast<bool>(*expression), "alignment fixture parses");
        Detail::PreparedMath paint(factory.get(), expression, 24.f);
        unsigned turns{};
        while (! paint.Advance())
            Require(++turns <= 16, "one bounded preparation event per text leaf");
        const auto& glyphs = paint.GlyphLayouts();
        const auto first   = [&](std::wstring_view text) { return std::ranges::find_if(glyphs, [&](const auto& glyph) { return glyph.node->text == text; }); };
        const auto x = first(L"x"), longX = first(L"xxxx");
        const auto equalities = std::ranges::subrange(std::ranges::find_if(glyphs, [](const auto& glyph) { return glyph.node->text == L"="; }), glyphs.end());
        const auto firstEquality = std::ranges::find_if(equalities.begin(), equalities.end(), [](const auto& glyph) { return glyph.node->text == L"="; });
        const auto secondEquality =
            firstEquality == equalities.end()
                ? glyphs.end()
                : std::ranges::find_if(std::next(firstEquality), glyphs.end(), [](const auto& glyph) { return glyph.node->text == L"="; });
        Require(glyphs.size() >= 6u && x != glyphs.end() && longX != glyphs.end() && firstEquality != glyphs.end() && secondEquality != glyphs.end() &&
                    x->origin.y < longX->origin.y && firstEquality->origin.y < secondEquality->origin.y,
                "two measured rows retain independently shaped operands, operators and numbers");
        if (environment == L"aligned")
        {
            Require(std::abs(firstEquality->origin.x - secondEquality->origin.x) < .01f, "aligned equality columns share the same left edge");
            Require(std::abs(x->origin.x + x->extent.width - longX->origin.x - longX->extent.width) < .01f, "aligned left operands share their right edge");
        }
        else if (environment == L"cases")
            Require(std::abs(x->origin.x - longX->origin.x) < .01f && std::abs(firstEquality->origin.x - secondEquality->origin.x) < .01f,
                    "cases align both expression and condition columns to the left");
        else
            Require(std::abs(x->origin.x + x->extent.width / 2 - longX->origin.x - longX->extent.width / 2) < .01f,
                    "ordinary matrices retain centered columns");
    }
    Require(! ParseMath(L"\\begin{gathered}a&b\\end{gathered}"), "gathered rows reject multiple columns");
}
void TestRichMathPreparedStyleSpacingScriptsAndLimits()
{
    using namespace DxUi::RichText;
    const auto factory = PreparedFlowchartTests::WriteFactory();
    const auto prepare = [&](std::wstring_view source, bool display = false)
    {
        auto expression = std::make_shared<const MathExpression>(ParseMath(source));
        Require(static_cast<bool>(*expression), "math geometry fixture parses before preparation");
        auto paint = std::make_unique<Detail::PreparedMath>(factory.get(), std::move(expression), 24.f, display);
        unsigned turns{};
        while (! paint->Advance())
            Require(++turns <= 512u, "math geometry preparation advances one bounded text leaf at a time");
        const auto size     = paint->Size();
        const auto baseline = paint->Baseline();
        Require(paint->Advance() && paint->Size().width == size.width && paint->Size().height == size.height && paint->Baseline() == baseline,
                "completed preparation is idempotent");
        return paint;
    };
    const auto styles       = prepare(L"x+12+\\sin(x)");
    const auto& styleGlyphs = styles->GlyphLayouts();
    const auto findText     = [](const auto& glyphs, std::wstring_view text)
    { return std::ranges::find_if(glyphs, [&](const auto& glyph) { return glyph.node->text == text; }); };
    const auto variable = findText(styleGlyphs, L"x"), digits = findText(styleGlyphs, L"12"), function = findText(styleGlyphs, L"sin");
    Require(variable != styleGlyphs.end() && variable->italic && digits != styleGlyphs.end() && ! digits->italic && function != styleGlyphs.end() &&
                ! function->italic,
            "variables use math italics while numeric runs and named operators stay upright");
    const auto upright = prepare(L"\\mathrm{x}");
    Require(upright->GlyphLayouts().size() == 1u && ! upright->GlyphLayouts()[0u].italic, "upright style overrides the ordinary-variable italic metadata");

    const auto binary = prepare(L"x+y"), ordinaryPlus = prepare(L"x\\mathord{+}y"), unary = prepare(L"+x");
    const auto& binaryGlyphs   = binary->GlyphLayouts();
    const auto& ordinaryGlyphs = ordinaryPlus->GlyphLayouts();
    const auto& unaryGlyphs    = unary->GlyphLayouts();
    const auto binaryPlus = findText(binaryGlyphs, L"+"), ordinaryPlusGlyph = findText(ordinaryGlyphs, L"+");
    const auto binaryFollowing = findText(binaryGlyphs, L"y"), ordinaryFollowing = findText(ordinaryGlyphs, L"y");
    Require(binaryPlus != binaryGlyphs.end() && ordinaryPlusGlyph != ordinaryGlyphs.end() && binaryFollowing != binaryGlyphs.end() &&
                ordinaryFollowing != ordinaryGlyphs.end() && unaryGlyphs.size() == 2u &&
                binaryFollowing->origin.x - (binaryPlus->origin.x + binaryPlus->extent.width) >
                    ordinaryFollowing->origin.x - (ordinaryPlusGlyph->origin.x + ordinaryPlusGlyph->extent.width) + 1.f,
            "the same plus glyph receives binary spacing in x+y and ordinary spacing in x\\mathord{+}y");
    Require(unaryGlyphs[1u].origin.x - (unaryGlyphs[0u].origin.x + unaryGlyphs[0u].extent.width) < 1.f,
            "a leading binary atom normalizes to ordinary and does not retain binary spacing");
    const auto negativeKern = prepare(L"\\!x"), plainVariable = prepare(L"x"), negativeBetween = prepare(L"x\\!y");
    const auto& kernGlyphs    = negativeKern->GlyphLayouts();
    const auto& betweenGlyphs = negativeBetween->GlyphLayouts();
    Require(kernGlyphs.size() == 1u && kernGlyphs[0u].origin.x >= 0.f &&
                kernGlyphs[0u].origin.x + kernGlyphs[0u].extent.width <= negativeKern->Size().width + .01f &&
                negativeKern->Size().width >= plainVariable->Size().width && betweenGlyphs.size() == 2u && betweenGlyphs[0u].origin.x >= 0.f &&
                betweenGlyphs[1u].origin.x >= betweenGlyphs[0u].origin.x + betweenGlyphs[0u].extent.width - 8.f &&
                betweenGlyphs[1u].origin.x + betweenGlyphs[1u].extent.width <= negativeBetween->Size().width + .01f,
            "negative thin space preserves tightening while leading ink and the final glyph remain within reserved formula geometry");

    const auto scripts = prepare(L"x_i^2"), obliqueScripts = prepare(L"f_i^2");
    const auto& scriptGlyphs = scripts->GlyphLayouts();
    Require(scriptGlyphs.size() == 3u, "script fixture shapes the base, subscript and superscript independently");
    DWRITE_TEXT_METRICS baseMetrics{};
    Require(SUCCEEDED(scriptGlyphs[0u].layout->GetMetrics(&baseMetrics)), "italic base layout exposes its shaped advance");
    Require(scriptGlyphs[1u].font < scriptGlyphs[0u].font && scriptGlyphs[2u].font < scriptGlyphs[0u].font,
            "subscripts and superscripts use reduced math styles");
    Require(scriptGlyphs[1u].origin.y + scriptGlyphs[1u].extent.above > scriptGlyphs[0u].origin.y + scriptGlyphs[0u].extent.above &&
                scriptGlyphs[2u].origin.y + scriptGlyphs[2u].extent.above < scriptGlyphs[0u].origin.y + scriptGlyphs[0u].extent.above,
            "script baselines clear the base baseline vertically");
    Require(std::abs(scriptGlyphs[2u].origin.x - scriptGlyphs[2u].leftOverhang - scriptGlyphs[0u].origin.x - baseMetrics.widthIncludingTrailingWhitespace -
                     scriptGlyphs[0u].italicCorrection) < .05f,
            "superscript advance includes the shaped base italic correction");
    Require(std::abs(scriptGlyphs[1u].origin.x - scriptGlyphs[1u].leftOverhang - (scriptGlyphs[0u].origin.x + baseMetrics.widthIncludingTrailingWhitespace)) <
                .05f,
            "subscript advance excludes the superscript italic correction");
    const auto mixedBase   = prepare(L"{\\mathit{x}\\mathrm{+}}^2");
    const auto mixedPlus   = findText(mixedBase->GlyphLayouts(), L"+");
    const auto mixedScript = findText(mixedBase->GlyphLayouts(), L"2");
    Require(mixedPlus != mixedBase->GlyphLayouts().end() && mixedScript != mixedBase->GlyphLayouts().end() &&
                mixedScript->origin.x >= mixedPlus->origin.x + mixedPlus->extent.width - .01f &&
                std::abs(mixedScript->origin.x - mixedPlus->origin.x - mixedPlus->extent.width - mixedPlus->italicCorrection) < .05f,
            "a grouped script anchor uses the final drawable operator instead of an earlier italic variable");
    const auto displayFraction = prepare(L"\\dfrac{x}{y}"), textFraction = prepare(L"\\tfrac{x}{y}");
    Require(displayFraction->GlyphLayouts().size() == 2u && textFraction->GlyphLayouts().size() == 2u &&
                displayFraction->GlyphLayouts()[0u].font > textFraction->GlyphLayouts()[0u].font &&
                displayFraction->Size().height > textFraction->Size().height,
            "dfrac and tfrac propagate distinct numerator and denominator styles and extents");
    const auto scriptStyle = prepare(L"\\scriptstyle x"), ordinaryStyle = prepare(L"x");
    Require(scriptStyle->GlyphLayouts().size() == 1u && ordinaryStyle->GlyphLayouts().size() == 1u &&
                scriptStyle->GlyphLayouts()[0u].font < ordinaryStyle->GlyphLayouts()[0u].font,
            "explicit scriptstyle changes the scoped formula font scale");
    const auto radical = prepare(L"\\sqrt{x}"), accented = prepare(L"\\hat{x}"), plain = prepare(L"x"), fenced = prepare(L"\\left(x\\right)"),
               sizedFence = prepare(L"\\Big(");
    Require(radical->Size().width > plain->Size().width && radical->Size().height > plain->Size().height && accented->Size().height > plain->Size().height &&
                fenced->Size().height > plain->Size().height && sizedFence->Size().height > plain->Size().height,
            "radical, accent and fixed or dynamic delimiters contribute their own measured geometry");

    const auto inlineSum = prepare(L"\\sum_{i}^{n}"), displaySum = prepare(L"\\sum_{i}^{n}", true), forcedLimits = prepare(L"\\sum\\limits_{i}^{n}");
    const auto& inlineGlyphs  = inlineSum->GlyphLayouts();
    const auto& displayGlyphs = displaySum->GlyphLayouts();
    const auto& forcedGlyphs  = forcedLimits->GlyphLayouts();
    Require(inlineGlyphs.size() == 3u && displayGlyphs.size() == 3u && forcedGlyphs.size() == 3u &&
                inlineGlyphs[1u].origin.x >= inlineGlyphs[0u].origin.x + inlineGlyphs[0u].extent.width - 1.f &&
                displayGlyphs[0u].font > inlineGlyphs[0u].font &&
                std::abs(displayGlyphs[1u].origin.x + displayGlyphs[1u].extent.width / 2.f -
                         (displayGlyphs[0u].origin.x + displayGlyphs[0u].extent.width / 2.f)) < 3.f &&
                std::abs(forcedGlyphs[2u].origin.x + forcedGlyphs[2u].extent.width / 2.f - (forcedGlyphs[0u].origin.x + forcedGlyphs[0u].extent.width / 2.f)) <
                    3.f &&
                displayGlyphs[1u].origin.y > inlineGlyphs[1u].origin.y && displayGlyphs[2u].origin.y < inlineGlyphs[2u].origin.y,
            "automatic limits stack only in display style while explicit limits always stack around the operator");

    const auto inlineIntegral = prepare(L"\\int", false), displayIntegral = prepare(L"\\int", true);
    Require(inlineIntegral->GlyphLayouts().size() == 1u && displayIntegral->GlyphLayouts().size() == 1u &&
                displayIntegral->GlyphLayouts()[0u].font > inlineIntegral->GlyphLayouts()[0u].font,
            "display integrals use a larger operator glyph even when their limits stay beside the operator");
    const auto normalMatrix    = prepare(L"\\begin{matrix}\\sum&x\\end{matrix}", true);
    const auto smallMatrix     = prepare(L"\\begin{smallmatrix}\\sum&x\\end{smallmatrix}", true);
    const auto displayOperator = prepare(L"\\sum", true);
    const auto matrixSum       = findText(normalMatrix->GlyphLayouts(), L"∑");
    const auto smallMatrixSum  = findText(smallMatrix->GlyphLayouts(), L"∑");
    Require(matrixSum != normalMatrix->GlyphLayouts().end() && smallMatrixSum != smallMatrix->GlyphLayouts().end() &&
                matrixSum->font < displayOperator->GlyphLayouts()[0u].font && smallMatrixSum->font < matrixSum->font,
            "matrix cells use text-style operators and smallmatrix applies a smaller cell style");

    const auto big = prepare(L"\\big("), Big = prepare(L"\\Big("), bigg = prepare(L"\\bigg("), Bigg = prepare(L"\\Bigg(");
    Require(big->Size().height < Big->Size().height && Big->Size().height < bigg->Size().height && bigg->Size().height < Bigg->Size().height,
            "sized delimiters increase their measured height in each size step");
    const auto invisible = prepare(L"\\left.x\\right."), plainWithDot = prepare(L"x"), emptyText = prepare(L"\\text{}");
    Require(invisible->GlyphLayouts().size() == 1u && plainWithDot->GlyphLayouts().size() == 1u &&
                std::abs(invisible->Size().width - plainWithDot->Size().width) < .01f &&
                std::abs(invisible->Size().height - plainWithDot->Size().height) < .01f && emptyText->GlyphLayouts().empty(),
            "invisible delimiters reserve no width or height and empty text prepares without a glyph layout");

    const auto tilde = prepare(L"\\widetilde{x}"), doubleDot = prepare(L"\\ddot{x}");
    const auto hasNonHorizontalStroke = [](const auto& strokes)
    { return std::ranges::any_of(strokes, [](const auto& line) { return std::abs(line.first.y - line.last.y) > .01f; }); };
    Require(hasNonHorizontalStroke(tilde->StrokeLayouts()) && doubleDot->StrokeLayouts().size() == 2u &&
                std::abs((doubleDot->StrokeLayouts()[0u].first.x + doubleDot->StrokeLayouts()[0u].last.x) / 2.f +
                         (doubleDot->StrokeLayouts()[1u].first.x + doubleDot->StrokeLayouts()[1u].last.x) / 2.f -
                         (doubleDot->GlyphLayouts()[0u].origin.x * 2.f + doubleDot->GlyphLayouts()[0u].extent.width +
                          doubleDot->GlyphLayouts()[0u].rightOverhang - doubleDot->GlyphLayouts()[0u].leftOverhang)) < .05f,
            "tilde draws a wave and double-dot accents remain symmetric around their base ink bounds");

    const auto escapedOpen = ParseMath(L"\\text{\\{}"), escapedClose = ParseMath(L"\\text{\\}}");
    Require(escapedOpen && escapedClose && escapedOpen.root.children[0u].text == L"{" && escapedClose.root.children[0u].text == L"}",
            "escaped braces stay text characters instead of changing group depth");

    const auto emptyTextScript = prepare(L"\\text{}^2"), emptyOperatorScript = prepare(L"\\operatorname{}^x");
    Require(emptyTextScript->GlyphLayouts().size() == 1u && emptyOperatorScript->GlyphLayouts().size() == 1u &&
                emptyTextScript->GlyphLayouts()[0u].node->text == L"2" && emptyOperatorScript->GlyphLayouts()[0u].node->text == L"x",
            "empty text and operator bases remain safe when their script anchor has no glyph leaf");

    const auto contained = [](const auto& paint)
    {
        const auto size = paint->Size();
        for (const auto& glyph : paint->GlyphLayouts())
            if (glyph.origin.x - glyph.leftOverhang < -.01f || glyph.origin.y - glyph.topOverhang < -.01f ||
                glyph.origin.x + glyph.extent.width + glyph.rightOverhang > size.width + .01f ||
                glyph.origin.y + glyph.extent.above + glyph.extent.below + glyph.bottomOverhang > size.height + .01f)
                return false;
        for (const auto& line : paint->StrokeLayouts())
        {
            const float half = line.thickness / 2.f;
            if (std::min(line.first.x, line.last.x) - half < -.01f || std::min(line.first.y, line.last.y) - half < -.01f ||
                std::max(line.first.x, line.last.x) + half > size.width + .01f || std::max(line.first.y, line.last.y) + half > size.height + .01f)
                return false;
        }
        return true;
    };
    Require(contained(scripts) && contained(obliqueScripts) && contained(radical) && contained(accented) && contained(fenced) && contained(tilde) &&
                contained(doubleDot),
            "prepared formula extents contain glyph overhang, accent ink and fence stroke widths");

    std::wstring oversizedFormula;
    for (unsigned term = 0u; term < 32u; ++term)
        oversizedFormula.append(L"\\sum+");
    const auto oversizedExpression = std::make_shared<const MathExpression>(ParseMath(oversizedFormula));
    Detail::PreparedMath retry(factory.get(), oversizedExpression, 4096.f);
    for (std::size_t leaf = 0u; leaf < retry.GlyphLayouts().size(); ++leaf)
        Require(! retry.Advance(), "oversized geometry fixture shapes each bounded glyph leaf before finalization");
    bool firstFailure{}, secondFailure{};
    try
    {
        retry.Advance();
    }
    catch (const std::length_error&)
    {
        firstFailure = true;
    }
    const float firstExtent = retry.Size().width;
    try
    {
        retry.Advance();
    }
    catch (const std::length_error&)
    {
        secondFailure = true;
    }
    Require(firstFailure && secondFailure && firstExtent > 65536.f && retry.Size().width == firstExtent,
            "failed geometry finalization can be retried without accumulating row extents or strokes");
}
void TestRichMathBoundedCompatibilityCorpus()
{
    NativeMathCorpusTests::Run(PreparedFlowchartTests::WriteFactory().get(), Require);
}
void TestRichMathRootIndexSharesParserBudget()
{
    using namespace DxUi::RichText;
    for (const auto source : {L"\\sqrt[\\sqrt[3]{x}]{y}", L"\\sqrt[{[a,b]}]{x}", L"\\sqrt[\\scriptstyle n+1]{x}"})
    {
        const auto expression = ParseMath(source);
        Require(expression && expression.root.children.size() == 1u && expression.root.children[0u].kind == MathKind::Root &&
                    expression.root.children[0u].children.size() == 2u,
                "an optional root degree consumes its own nested grammar instead of the first raw closing bracket");
        Detail::PreparedMath paint(PreparedFlowchartTests::WriteFactory().get(), std::make_shared<const MathExpression>(expression), 24.f);
        unsigned turns{};
        while (! paint.Advance())
            Require(++turns <= 512u, "nested root degrees prepare within the shared formula budget");
    }
    std::wstring degree = L"3";
    for (unsigned level = 0u; level < 32u; ++level)
        degree = L"\\sqrt[" + degree + L"]{x}";
    Require(! ParseMath(degree), "optional root degrees do not restart the global nesting allowance");
    std::wstring wideDegree = L"\\sqrt[";
    for (unsigned atom = 0u; atom < 260u; ++atom)
        wideDegree += L"x+";
    wideDegree += L"y]{x}";
    Require(! ParseMath(wideDegree), "optional root degrees do not restart the global syntax-node allowance");
}
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
    const auto oversizedSource = L"$" + std::wstring(5000u, L'x') + L"_y_$ **literal tail**";
    const auto oversized       = RichSemanticDocumentTests::Build(oversizedSource, MarkdownProfile::RichMarkdown);
    Require(! oversized->Diagnostics().empty() && oversized->Map().Display().starts_with(oversizedSource) && oversized->Map().Source() == oversizedSource,
            "over-capacity notation retains the entire literal line without interpreting nested Markdown");
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
    DXUI_RUN_TEST(TestRichMathTextWhitespaceAndPreparation);
    DXUI_RUN_TEST(TestRichMathRejectedObjectOwnsItsDiagnostic);
    DXUI_RUN_TEST(TestRichMathLimitModifiersAndSymbolIdentity);
    DXUI_RUN_TEST(TestRichMathExplicitDelimiterRecognition);
    DXUI_RUN_TEST(TestRichMathAlignedCasesAndMatrixGeometry);
    DXUI_RUN_TEST(TestRichMathPreparedStyleSpacingScriptsAndLimits);
    DXUI_RUN_TEST(TestRichMathBoundedCompatibilityCorpus);
    DXUI_RUN_TEST(TestRichMathRootIndexSharesParserBudget);
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
