#include "Controls.Tests.RichSemanticDocument.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.MarkdownCorpus.h"
#include "Controls/DxUi.NativeFlowchart.h"
#include "Controls/DxUi.PreparedMath.h"
#include "Support.Tests.Footnotes.h"
#include "Support.Tests.MathCorpus.h"
#include "Support.Tests.RichSemanticDocument.h"
#include "Support.Tests.SemanticDiagrams.h"
#include "Support.Tests.SemanticSourceBlock.h"
#include <DxUi/RichMath.h>
#include <iterator>
#include <tuple>
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
    const auto boldText   = prepare(L"\\textbf{bold text}");
    const auto italicText = prepare(L"\\textit{italic text}");
    Require(boldText->GlyphLayouts().size() == 1u && boldText->GlyphLayouts()[0u].node->text == L"bold text" && boldText->GlyphLayouts()[0u].bold &&
                ! boldText->GlyphLayouts()[0u].italic && italicText->GlyphLayouts().size() == 1u &&
                italicText->GlyphLayouts()[0u].node->text == L"italic text" && ! italicText->GlyphLayouts()[0u].bold && italicText->GlyphLayouts()[0u].italic,
            "text style commands retain their text and select the requested native font style");
    Require(! ParseMath(L"\\textbf{unclosed"), "malformed styled text rejects the entire formula");
    for (const auto& [source, left, right] : {std::tuple{L"\\left\\uparrow x\\right\\downarrow", L'↑', L'↓'},
                                              std::tuple{L"\\left\\updownarrow x\\right\\Updownarrow", L'↕', L'⇕'},
                                              std::tuple{L"\\left\\Uparrow x\\right\\Downarrow", L'⇑', L'⇓'}})
    {
        const auto expression = ParseMath(source);
        Require(expression && expression.root.children.size() == 1u && expression.root.children[0u].kind == MathKind::Delimited &&
                    expression.root.children[0u].text == std::wstring{left, right},
                "vertical arrow delimiters retain exact independent symbols");
        const auto arrows = prepare(source);
        Require(arrows->Size().width > 0.f && arrows->Size().height > 0.f, "vertical arrow fences have measured native geometry");
    }
    const auto sizedArrow = prepare(L"\\Big\\updownarrow");
    Require(sizedArrow->Size().height > 0.f, "explicitly sized vertical arrow delimiters prepare");
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto previousTable = Detail::testRichMathTableOverride;
        const auto restoreTable  = wil::scope_exit([previousTable]() noexcept { Detail::testRichMathTableOverride = previousTable; });
        const std::array<std::uint8_t, 1u> missingTable{0u};
        Detail::testRichMathTableOverride = missingTable;
        for (const auto& [source, shafts, up, down] : {std::tuple{L"\\Big\\uparrow", 1u, true, false},
                                                       std::tuple{L"\\Big\\downarrow", 1u, false, true},
                                                       std::tuple{L"\\Big\\updownarrow", 1u, true, true},
                                                       std::tuple{L"\\Big\\Uparrow", 2u, true, false},
                                                       std::tuple{L"\\Big\\Downarrow", 2u, false, true},
                                                       std::tuple{L"\\Big\\Updownarrow", 2u, true, true}})
        {
            const auto fallback = prepare(source);
            const auto& lines   = fallback->StrokeLayouts();
            Require(fallback->VariantLayouts().empty() && lines.size() == shafts + (up ? 2u : 0u) + (down ? 2u : 0u),
                    "missing MATH table draws the correct number of arrow shafts and heads");
            for (std::size_t index = 0u; index < shafts; ++index)
                Require(lines[index].first.x == lines[index].last.x && lines[index].first.y < lines[index].last.y, "arrow fallback has vertical shafts");
            if (up)
                Require(lines[shafts].first.y > lines[shafts].last.y && lines[shafts + 1u].first.y < lines[shafts + 1u].last.y &&
                            lines[shafts].last.x == lines[shafts + 1u].first.x && lines[shafts].last.y == lines[shafts + 1u].first.y,
                        "upper arrow head joins at its upper tip");
            if (down)
            {
                const auto first = shafts + (up ? 2u : 0u);
                Require(lines[first].first.y < lines[first].last.y && lines[first + 1u].first.y > lines[first + 1u].last.y &&
                            lines[first].last.x == lines[first + 1u].first.x && lines[first].last.y == lines[first + 1u].first.y,
                        "lower arrow head joins at its lower tip");
            }
        }
    }
#endif

    for (const auto& [source, annotation, above] :
         {std::tuple{L"\\overset{a}{=}", L"a", true}, std::tuple{L"\\underset{b}{=}", L"b", false}, std::tuple{L"\\stackrel{c}{=}", L"c", true}})
    {
        const auto arranged = prepare(source);
        const auto& glyphs  = arranged->GlyphLayouts();
        const auto label = findText(glyphs, annotation), base = findText(glyphs, L"=");
        Require(label != glyphs.end() && base != glyphs.end(), "stacked notation shapes its base and annotation");
        const auto labelBaseline = label->origin.y + label->extent.above;
        const auto baseBaseline  = base->origin.y + base->extent.above;
        Require(above ? labelBaseline < baseBaseline : labelBaseline > baseBaseline,
                "over and under annotations are placed on the requested side of their base");
    }
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
    const auto scalableFence = prepare(L"\\left(\\frac{1}{\\frac{x}{y}}\\right)");
    const auto fenceSize     = scalableFence->Size();
    for (const auto& variant : scalableFence->VariantLayouts())
        Require(variant.left >= -.01f && variant.top >= -.01f && variant.left + variant.width <= fenceSize.width + .01f &&
                    variant.top + variant.height <= fenceSize.height + .01f && variant.glyph != 0u,
                "font-selected scalable delimiter ink remains inside prepared bounds");
    Require(scalableFence->VariantLayouts().size() * 3u + scalableFence->StrokeLayouts().size() >= 8u,
            "scalable parentheses use font variants when available or deterministic vector fallback");
    const auto isolatedFence = prepare(L"\\Big(");
#if DXUI_ENABLE_DIAGNOSTICS
    if (! isolatedFence->VariantLayouts().empty())
    {
        std::array<std::uint8_t, 232u> fixture{};
        const auto setValue = [&](std::size_t offset, std::uint16_t value)
        {
            fixture[offset]      = static_cast<std::uint8_t>(value >> 8u);
            fixture[offset + 1u] = static_cast<std::uint8_t>(value & 0xffu);
        };
        setValue(0u, 1u);
        setValue(4u, 10u); // version and MathConstants parent
        setValue(46u, 3200u);
        setValue(48u, 190u); // superscript shift and relative Device table
        setValue(200u, 24u);
        setValue(202u, 48u);
        setValue(204u, 3u); // 8-bit deltas over ppem 24..48
        setValue(206u, 0x0100u);
        setValue(230u, 0xff00u); // +1 pixel at 24, -1 pixel at 48
        const auto priorTable   = DxUi::RichText::Detail::testRichMathTableOverride;
        const auto restoreTable = wil::scope_exit([priorTable]() noexcept { DxUi::RichText::Detail::testRichMathTableOverride = priorTable; });
        DxUi::RichText::Detail::testRichMathTableOverride = fixture;
        const auto value                                  = std::make_shared<const MathExpression>(ParseMath(L"\\mathrm{x}^2"));
        const auto beforeReads                            = DxUi::RichText::Detail::testRichMathConstantReads;
        DxUi::RichText::Detail::PreparedMath lowDpi(factory.get(), value, 24.f, false, nullptr, 96.f);
        while (! lowDpi.Advance())
        {
        }
        DxUi::RichText::Detail::PreparedMath highDpi(factory.get(), value, 24.f, false, &lowDpi, 192.f);
        while (! highDpi.Advance())
        {
        }
        const auto& lowGlyphs  = lowDpi.GlyphLayouts();
        const auto& highGlyphs = highDpi.GlyphLayouts();
        Require(lowGlyphs.size() == 2u && highGlyphs.size() == 2u, "device-corrected script fixture retains both source glyphs");
        const auto lowShift  = lowGlyphs[1u].origin.y - lowGlyphs[0u].origin.y;
        const auto highShift = highGlyphs[1u].origin.y - highGlyphs[0u].origin.y;
        Require(std::abs(highShift - lowShift - 1.5f) < .02f, "actual script placement converts independent ppem device deltas at 96 and 192 DPI");
        const auto nestedExpression = std::make_shared<const MathExpression>(ParseMath(L"\\mathrm{x}^{\\mathrm{y}^2}"));
        DxUi::RichText::Detail::PreparedMath nestedLow(factory.get(), nestedExpression, 24.f, false, &lowDpi, 96.f);
        DxUi::RichText::Detail::PreparedMath nestedHigh(factory.get(), nestedExpression, 24.f, false, &lowDpi, 192.f);
        while (! nestedLow.Advance())
        {
        }
        while (! nestedHigh.Advance())
        {
        }
        const auto& nestedLowGlyphs  = nestedLow.GlyphLayouts();
        const auto& nestedHighGlyphs = nestedHigh.GlyphLayouts();
        Require(nestedLowGlyphs.size() == 3u && nestedHighGlyphs.size() == 3u, "nested device fixture retains all three glyphs");
        const auto nestedDelta =
            (nestedHighGlyphs[2u].origin.y - nestedHighGlyphs[1u].origin.y) - (nestedLowGlyphs[2u].origin.y - nestedLowGlyphs[1u].origin.y);
        Require(std::abs(nestedDelta) < .02f, "nested scripts use their own ppem instead of scaling the root's unrelated device delta");
        Require(DxUi::RichText::Detail::testRichMathConstantReads == beforeReads + 1u, "DPI-specific math values reuse one owned font-table snapshot");
    }
#endif
    if (! isolatedFence->VariantLayouts().empty())
    {
#if DXUI_ENABLE_DIAGNOSTICS
        const auto previousGlyphFailure = DxUi::RichText::Detail::testRichMathGlyphMetricsFailure;
        const auto restoreGlyphFailure =
            wil::scope_exit([previousGlyphFailure]() noexcept { DxUi::RichText::Detail::testRichMathGlyphMetricsFailure = previousGlyphFailure; });
        auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\Big("));
        DxUi::RichText::Detail::PreparedMath failedMetrics(factory.get(), expression, 24.f);
        DxUi::RichText::Detail::testRichMathGlyphMetricsFailure = E_OUTOFMEMORY;
        HRESULT failureCode                                     = S_OK;
        try
        {
            while (! failedMetrics.Advance())
            {
            }
        }
        catch (const wil::ResultException& failure)
        {
            failureCode = failure.GetErrorCode();
        }
        Require(failureCode == E_OUTOFMEMORY && DxUi::RichText::Detail::testRichMathGlyphMetricsFailure == S_OK,
                "font glyph metrics preserve transient allocation HRESULT instead of publishing fallback geometry");
        DxUi::RichText::Detail::PreparedMath recoveredMetrics(factory.get(), expression, 24.f);
        while (! recoveredMetrics.Advance())
        {
        }
        Require(! recoveredMetrics.VariantLayouts().empty(), "fresh formula preparation retries the same font glyph after a transient failure");
        DxUi::RichText::Detail::PreparedMath unsupportedMetrics(factory.get(), expression, 24.f);
        DxUi::RichText::Detail::testRichMathGlyphMetricsFailure = DWRITE_E_UNSUPPORTEDOPERATION;
        while (! unsupportedMetrics.Advance())
        {
        }
        Require(unsupportedMetrics.VariantLayouts().empty() && ! unsupportedMetrics.StrokeLayouts().empty(),
                "the explicitly unsupported font operation retains the bounded vector fallback");
        const auto previousIndexFailure = DxUi::RichText::Detail::testRichMathGlyphIndexFailure;
        const auto restoreIndexFailure =
            wil::scope_exit([previousIndexFailure]() noexcept { DxUi::RichText::Detail::testRichMathGlyphIndexFailure = previousIndexFailure; });
        for (const auto source : {L"x", L"\\Big("})
        {
            auto indexExpression = std::make_shared<const MathExpression>(ParseMath(source));
            DxUi::RichText::Detail::PreparedMath failedIndex(factory.get(), indexExpression, 24.f);
            DxUi::RichText::Detail::testRichMathGlyphIndexFailure = E_OUTOFMEMORY;
            HRESULT indexFailure                                  = S_OK;
            try
            {
                while (! failedIndex.Advance())
                {
                }
            }
            catch (const wil::ResultException& failure)
            {
                indexFailure = failure.GetErrorCode();
            }
            Require(indexFailure == E_OUTOFMEMORY && DxUi::RichText::Detail::testRichMathGlyphIndexFailure == S_OK,
                    "italic correction and delimiter glyph-index failures preserve the transient HRESULT");
        }
#endif
        Require(isolatedFence->StrokeLayouts().empty(), "the isolated font delimiter has no vector fallback strokes");
        wil::com_ptr<IWICImagingFactory> imaging;
        THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(128u, 128u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        const auto drawing = PreparedFlowchartTests::DrawingFactory();
        wil::com_ptr<ID2D1RenderTarget> target;
        const auto properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 1.f), brush.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
        isolatedFence->Draw(target.get(), brush.get(), D2D1::Point2F(8.f, 8.f), 1.f);
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(128u * 128u * 4u);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, 128u * 4u, static_cast<UINT>(pixels.size()), pixels.data()));
        bool visible{};
        for (std::size_t offset = 3u; offset < pixels.size(); offset += 4u)
            visible = visible || pixels[offset] != 0u;
        Require(visible, "a selected OpenType delimiter variant paints visible pixels to an offscreen target");
    }
    else
        Require(! isolatedFence->StrokeLayouts().empty(), "a font without an adequate variant retains the vector fallback");
    const auto horizontalAccent = prepare(L"\\vec{abcdefghijk}");
#if DXUI_ENABLE_DIAGNOSTICS
    const auto uprightAccent = prepare(L"\\vec{\\mathrm{abcdefghijk}}");
    if (! uprightAccent->VariantLayouts().empty())
    {
        const auto oldMetricFailure      = DxUi::RichText::Detail::testRichMathGlyphMetricsFailure;
        const auto oldIndexFailure       = DxUi::RichText::Detail::testRichMathGlyphIndexFailure;
        const auto restoreAccentFailures = wil::scope_exit([oldMetricFailure, oldIndexFailure]() noexcept
        {
            DxUi::RichText::Detail::testRichMathGlyphMetricsFailure = oldMetricFailure;
            DxUi::RichText::Detail::testRichMathGlyphIndexFailure   = oldIndexFailure;
        });
        const auto accentExpression      = std::make_shared<const MathExpression>(ParseMath(L"\\vec{\\mathrm{abcdefghijk}}"));
        for (const bool failMetric : {false, true})
        {
            DxUi::RichText::Detail::PreparedMath interrupted(factory.get(), accentExpression, 24.f);
            if (failMetric)
                DxUi::RichText::Detail::testRichMathGlyphMetricsFailure = E_OUTOFMEMORY;
            else
                DxUi::RichText::Detail::testRichMathGlyphIndexFailure = E_OUTOFMEMORY;
            HRESULT failureCode = S_OK;
            try
            {
                while (! interrupted.Advance())
                {
                }
            }
            catch (const wil::ResultException& failure)
            {
                failureCode = failure.GetErrorCode();
            }
            Require(failureCode == E_OUTOFMEMORY, "horizontal font lookup preserves transient glyph failure for owner retry");
            DxUi::RichText::Detail::PreparedMath retry(factory.get(), accentExpression, 24.f);
            while (! retry.Advance())
            {
            }
            Require(! retry.VariantLayouts().empty(), "horizontal accent retries successfully after transient failure");
        }
    }
#endif
    const auto accentSize = horizontalAccent->Size();
    float accentLeft = accentSize.width, accentRight{}, accentTop = accentSize.height, accentBottom{};
    for (const auto& part : horizontalAccent->VariantLayouts())
    {
        Require(part.left >= -.01f && part.top >= -.01f && part.left + part.width <= accentSize.width + .01f &&
                    part.top + part.height <= accentSize.height + .01f,
                "horizontal font accent ink remains within prepared formula bounds");
        accentLeft   = std::min(accentLeft, part.left);
        accentRight  = std::max(accentRight, part.left + part.width);
        accentTop    = std::min(accentTop, part.top);
        accentBottom = std::max(accentBottom, part.top + part.height);
    }
    if (horizontalAccent->VariantLayouts().size() > 2u && accentSize.width < 240.f && accentSize.height < 100.f)
    {
        wil::com_ptr<IWICImagingFactory> imaging;
        THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(256u, 128u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        const auto drawing = PreparedFlowchartTests::DrawingFactory();
        wil::com_ptr<ID2D1RenderTarget> target;
        const auto properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 1.f), brush.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
        horizontalAccent->Draw(target.get(), brush.get(), D2D1::Point2F(8.f, 8.f), 1.f);
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(256u * 128u * 4u);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, 256u * 4u, static_cast<UINT>(pixels.size()), pixels.data()));
        unsigned columns{}, gap{}, maximumGap{};
        for (auto column = static_cast<unsigned>(8.f + std::ceil(accentLeft)) + 2u; column + 2u < static_cast<unsigned>(8.f + std::floor(accentRight));
             ++column)
        {
            bool hit{};
            for (auto row = static_cast<unsigned>(8.f + std::floor(accentTop)); row <= static_cast<unsigned>(8.f + std::ceil(accentBottom)); ++row)
                hit = hit || pixels[(static_cast<std::size_t>(row) * 256u + column) * 4u + 3u] != 0u;
            if (hit)
            {
                ++columns;
                gap = 0u;
            }
            else
                maximumGap = std::max(maximumGap, ++gap);
        }
        Require(columns > 20u && maximumGap <= 4u, "horizontal font assembly connectors paint without visible internal gaps");
    }
    else
        Require(! horizontalAccent->VariantLayouts().empty() || ! horizontalAccent->StrokeLayouts().empty(),
                "fonts without a compatible horizontal assembly retain the bounded vector fallback");
    std::wstring tallFenceSource = L"\\left(\\begin{gathered}";
    for (unsigned row = 0u; row < 16u; ++row)
    {
        if (row)
            tallFenceSource += L"\\\\";
        tallFenceSource += L'x';
    }
    tallFenceSource += L"\\end{gathered}\\right)";
    const auto tallFence = prepare(tallFenceSource.c_str());
    const auto tallSize  = tallFence->Size();
    for (const auto& part : tallFence->VariantLayouts())
        Require(part.left >= -.01f && part.top >= -.01f && part.left + part.width <= tallSize.width + .01f && part.top + part.height <= tallSize.height + .01f,
                "every font assembly part stays inside the complete formula bounds");
    if (tallFence->VariantLayouts().size() > 2u && tallSize.width < 200.f && tallSize.height < 780.f)
    {
        wil::com_ptr<IWICImagingFactory> imaging;
        THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
        wil::com_ptr<IWICBitmap> bitmap;
        THROW_IF_FAILED(imaging->CreateBitmap(256u, 800u, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
        const auto drawing = PreparedFlowchartTests::DrawingFactory();
        wil::com_ptr<ID2D1RenderTarget> target;
        const auto properties =
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(bitmap.get(), properties, target.put()));
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(1.f, 1.f, 1.f, 1.f), brush.put()));
        target->BeginDraw();
        target->Clear(D2D1::ColorF(0.f, 0.f, 0.f, 0.f));
        tallFence->Draw(target.get(), brush.get(), D2D1::Point2F(8.f, 8.f), 1.f);
        THROW_IF_FAILED(target->EndDraw());
        std::vector<BYTE> pixels(256u * 800u * 4u);
        THROW_IF_FAILED(bitmap->CopyPixels(nullptr, 256u * 4u, static_cast<UINT>(pixels.size()), pixels.data()));
        unsigned inkRows{}, gap{}, maxGap{};
        const auto right = static_cast<unsigned>(8.f + tallSize.width * .35f);
        for (unsigned y = 25u; y < static_cast<unsigned>(tallSize.height) - 10u; ++y)
        {
            bool hit{};
            for (unsigned x = 8u; x < right; ++x)
                hit = hit || pixels[(static_cast<std::size_t>(y) * 256u + x) * 4u + 3u] != 0u;
            if (hit)
            {
                ++inkRows;
                gap = 0u;
            }
            else
                maxGap = std::max(maxGap, ++gap);
        }
        Require(inkRows > 200u && maxGap <= 4u, "assembled delimiter extenders join without visible gaps in offscreen raster output");
    }
    std::wstring manyFences(0u, L' ');
    for (unsigned index = 0u; index < 14u; ++index)
        manyFences += tallFenceSource;
    const auto boundedFences = prepare(manyFences);
    Require(boundedFences->VariantLayouts().size() <= 512u,
            "many oversized delimiters obey the per-formula assembly-part budget and use vector fallback when exhausted");
    const auto displayFraction = prepare(L"\\dfrac{x}{y}"), textFraction = prepare(L"\\tfrac{x}{y}");
    Require(displayFraction->GlyphLayouts().size() == 2u && textFraction->GlyphLayouts().size() == 2u &&
                displayFraction->GlyphLayouts()[0u].font > textFraction->GlyphLayouts()[0u].font &&
                displayFraction->Size().height > textFraction->Size().height,
            "dfrac and tfrac propagate distinct numerator and denominator styles and extents");
    const auto scriptStyle = prepare(L"\\scriptstyle x"), ordinaryStyle = prepare(L"x");
    Require(scriptStyle->GlyphLayouts().size() == 1u && ordinaryStyle->GlyphLayouts().size() == 1u &&
                scriptStyle->GlyphLayouts()[0u].font < ordinaryStyle->GlyphLayouts()[0u].font,
            "explicit scriptstyle changes the scoped formula font scale");
    const auto boldSymbols = prepare(L"\\boldsymbol{x+1}"), normalSymbols = prepare(L"\\mathbf{\\mathnormal{x+1}}");
    Require(boldSymbols->GlyphLayouts().size() == 3u && boldSymbols->GlyphLayouts()[0u].bold && boldSymbols->GlyphLayouts()[0u].italic &&
                boldSymbols->GlyphLayouts()[1u].bold && ! boldSymbols->GlyphLayouts()[1u].italic && boldSymbols->GlyphLayouts()[2u].bold &&
                ! boldSymbols->GlyphLayouts()[2u].italic && normalSymbols->GlyphLayouts().size() == 3u && ! normalSymbols->GlyphLayouts()[0u].bold &&
                normalSymbols->GlyphLayouts()[0u].italic && ! normalSymbols->GlyphLayouts()[1u].italic,
            "bold symbols preserve ordinary math italics while mathnormal restores scoped default weight and slant");
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
    const auto raggedMatrix = prepare(L"\\begin{matrix}a&b\\\\c\\end{matrix}");
    Require(raggedMatrix->GlyphLayouts().size() == 3u && raggedMatrix->Size().width > 0.f && raggedMatrix->Size().height > 0.f,
            "a ragged matrix prepares visible cells and an implicit empty cell");
    const auto substack = prepare(L"\\substack{i=0\\\\j=1}");
    Require(substack->Size().height > ordinaryStyle->Size().height && ! substack->GlyphLayouts().empty(),
            "substack publishes distinct measured rows in native geometry");
    const auto slashFences = prepare(L"\\left/\\frac{x}{y}\\right\\backslash");
    Require(slashFences->Size().width > 0.f && slashFences->Size().height > 0.f &&
                slashFences->VariantLayouts().size() + slashFences->StrokeLayouts().size() >= 2u,
            "slash and backslash fences have retained native geometry");

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
    for (unsigned level = 0u; level <= MathMaximumGroupDepth; ++level)
        degree = L"\\sqrt[" + degree + L"]{x}";
    Require(! ParseMath(degree), "optional root degrees do not restart the global nesting allowance");
    std::wstring wideDegree = L"\\sqrt[";
    for (std::size_t atom = 0u; atom <= MathMaximumNodes / 2u; ++atom)
        wideDegree += L"x+";
    wideDegree += L"y]{x}";
    Require(! ParseMath(wideDegree), "optional root degrees do not restart the global syntax-node allowance");
}
void TestRichSemanticNativeMathematicsAndBroaderMermaid()
{
    using namespace DxUi::RichText;
    static_assert(! std::is_nothrow_default_constructible_v<MathNode> && ! std::is_nothrow_default_constructible_v<MathExpression>);
    const std::wstring source = L"Before $a_i^2+\\frac{1}{2}$ after.\r\n\r\n"
                                L"$$\r\n\\begin{bmatrix}a&b\\\\c&d\\end{bmatrix}\r\n$$\r\n\r\n"
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
                               L"\\begin{matrix}a&b\\\\c\\end{matrix}",
                               L"\\text{speed in m/s}",
                               L"\\int_0^1 x dx",
                               L"\\left( a \\rightarrow b \\right)",
                               L"\\mathbb{R}+\\mathcal{F}+\\mathfrak{ABCxyz}"})
        Require(static_cast<bool>(ParseMath(formula)), "documented mathematical notation parses");
    const auto fraktur = ParseMath(L"\\mathfrak{CHIRZaz09}");
    Require(static_cast<bool>(fraktur), "Fraktur alphabet parses");
    std::wstring frakturText;
    const auto collect = [&frakturText](const auto& self, const MathNode& node) -> void
    {
        if (node.kind == MathKind::Text)
            frakturText += node.text;
        for (const auto& child : node.children)
            self(self, child);
    };
    collect(collect, fraktur.root);
    Require(frakturText == L"ℭℌℑℜℨ𝔞𝔷09", "Fraktur maps Unicode legacy capitals and supplementary lowercase without changing digits");
    const auto writeFactory = PreparedFlowchartTests::WriteFactory();
    Detail::PreparedMath frakturPaint(writeFactory.get(), std::make_shared<const MathExpression>(fraktur), 24.f);
    unsigned frakturTurns{};
    while (! frakturPaint.Advance())
        Require(++frakturTurns <= 32u, "Fraktur glyphs prepare within the bounded leaf budget");
    Require(! frakturPaint.GlyphLayouts().empty() && frakturPaint.Size().width > 0.f && frakturPaint.Size().height > 0.f,
            "Fraktur participates in native measured layout");
    for (const auto& [formula, expected] : {std::pair{L"\\mathsf{Az09}", L"𝖠𝗓𝟢𝟫"}, std::pair{L"\\mathtt{Az09}", L"𝙰𝚣𝟶𝟿"}})
    {
        const auto alphabet = ParseMath(formula);
        Require(static_cast<bool>(alphabet), "sans and monospace mathematical alphabets parse");
        frakturText.clear();
        collect(collect, alphabet.root);
        Require(frakturText == expected, "sans and monospace map letters and digits to their independent Unicode alphabet");
        Detail::PreparedMath alphabetPaint(writeFactory.get(), std::make_shared<const MathExpression>(alphabet), 24.f);
        unsigned turns{};
        while (! alphabetPaint.Advance())
            Require(++turns <= 32u, "mathematical alphabet prepares within its bounded leaf budget");
        Require(! alphabetPaint.GlyphLayouts().empty() && alphabetPaint.Size().width > 0.f, "mathematical alphabet participates in native layout");
    }
    for (const auto formula : {L"\\input{file}", L"\\frac{a}", L"x^2^3", L"\\begin{matrix}x\\end{cases}"})
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
    for (const auto& [body, expected] : {std::pair{L"classDiagram\nnote \"Standalone note\"\n", L"Note: Standalone note"},
                                         std::pair{L"classDiagram\nclass A\nnote for A \"Attached note\"\n", L"A note: Attached note"}})
    {
        const auto sourceWithNote = L"```mermaid\n" + std::wstring(body) + L"```\n";
        const auto semantic       = RichSemanticDocumentTests::Build(sourceWithNote, MarkdownProfile::RichMarkdown);
        const auto diagram        = std::ranges::find(semantic->Elements(), SemanticRole::Diagram, &SemanticElement::role);
        Require(semantic->Map().Source() == sourceWithNote && diagram != semantic->Elements().end() && diagram->name.find(expected) != std::wstring::npos,
                "class notes preserve exact source and expose their content in the diagram alternative");
    }
    const std::wstring namespaceSource = L"```mermaid\nclassDiagram\nnamespace Domain {\nclass A\nclass B\nA --> B\n}\n```\n";
    const auto namespaceDocument       = RichSemanticDocumentTests::Build(namespaceSource, MarkdownProfile::RichMarkdown);
    const auto namespaceDiagram        = std::ranges::find(namespaceDocument->Elements(), SemanticRole::Diagram, &SemanticElement::role);
    Require(namespaceDocument->Map().Source() == namespaceSource && namespaceDiagram != namespaceDocument->Elements().end() &&
                namespaceDiagram->name.find(L"Domain") != std::wstring::npos && namespaceDiagram->name.find(L"A") != std::wstring::npos &&
                namespaceDiagram->name.find(L"B") != std::wstring::npos,
            "class namespace semantic alternative exposes the namespace and members while retaining exact source");
    const std::wstring genericSource = L"```mermaid\nclassDiagram\nclass Box {\n+List~int~ values\n}\n```\n";
    const auto genericDocument       = RichSemanticDocumentTests::Build(genericSource, MarkdownProfile::RichMarkdown);
    const auto genericDiagram        = std::ranges::find(genericDocument->Elements(), SemanticRole::Diagram, &SemanticElement::role);
    Require(genericDocument->Map().Source() == genericSource && genericDiagram != genericDocument->Elements().end() &&
                genericDiagram->name.find(L"List<int>") != std::wstring::npos,
            "class generic semantic alternative exposes the displayed type and preserves exact source");
    const std::wstring sequenceSource = L"```mermaid\nsequenceDiagram\nparticipant A as Alpha\nparticipant B as Beta\nA->>B: hello\nB-->>A: reply\n```\n";
    const auto sequenceDocument       = RichSemanticDocumentTests::Build(sequenceSource, MarkdownProfile::RichMarkdown);
    const auto sequenceDiagram        = std::ranges::find(sequenceDocument->Elements(), SemanticRole::Diagram, &SemanticElement::role);
    Require(sequenceDocument->Map().Source() == sequenceSource && sequenceDiagram != sequenceDocument->Elements().end() &&
                sequenceDiagram->name.starts_with(L"Sequence diagram") && sequenceDiagram->name.find(L"Alpha") != std::wstring::npos &&
                sequenceDiagram->name.find(L"Beta") != std::wstring::npos && sequenceDiagram->name.find(L"hello") != std::wstring::npos &&
                sequenceDiagram->name.find(L"reply") > sequenceDiagram->name.find(L"hello"),
            "sequence semantic alternative retains participants and chronological messages with exact source");
    const std::wstring pieSource = L"```mermaid\npie showData title Inventory\n\"One\": 25\n\"Three\": 75\n```\n";
    const auto pieDocument       = RichSemanticDocumentTests::Build(pieSource, MarkdownProfile::RichMarkdown);
    const auto pieDiagram        = std::ranges::find(pieDocument->Elements(), SemanticRole::Diagram, &SemanticElement::role);
    Require(pieDocument->Map().Source() == pieSource && pieDiagram != pieDocument->Elements().end() && pieDiagram->name.starts_with(L"Pie chart: Inventory") &&
                pieDiagram->name.find(L"One: 25") != std::wstring::npos && pieDiagram->name.find(L"Three: 75") != std::wstring::npos,
            "pie semantic alternative preserves title, slice values, order and exact source");
    const std::wstring erSource = L"```mermaid\nerDiagram\nCUSTOMER ||--o{ ORDER : places\n```\n";
    const auto erDocument       = RichSemanticDocumentTests::Build(erSource, MarkdownProfile::RichMarkdown);
    const auto erDiagram        = std::ranges::find(erDocument->Elements(), SemanticRole::Diagram, &SemanticElement::role);
    Require(erDocument->Map().Source() == erSource && erDiagram != erDocument->Elements().end() &&
                erDiagram->name.starts_with(L"Entity relationship diagram") && erDiagram->name.find(L"CUSTOMER [1]") != std::wstring::npos &&
                erDiagram->name.find(L"ORDER [0..*]") != std::wstring::npos,
            "ER semantic alternative exposes both multiplicities with exact source");
    const std::wstring ganttSource = L"```mermaid\ngantt\ntitle Release\nsection Build\nCompile : 2024-02-27, 3d\n```\n";
    const auto ganttDocument       = RichSemanticDocumentTests::Build(ganttSource, MarkdownProfile::RichMarkdown);
    const auto ganttDiagram        = std::ranges::find(ganttDocument->Elements(), SemanticRole::Diagram, &SemanticElement::role);
    Require(ganttDocument->Map().Source() == ganttSource && ganttDiagram != ganttDocument->Elements().end() &&
                ganttDiagram->name.starts_with(L"Gantt chart: Release") && ganttDiagram->name.find(L"Build / Compile: 2024-02-27, 3d") != std::wstring::npos,
            "Gantt semantic alternative preserves title, section, dates and exact source");
    const std::wstring mindSource = L"```mermaid\nmindmap\n  root((Central))\n    First\n      Leaf\n    Second\n```\n";
    const auto mindDocument       = RichSemanticDocumentTests::Build(mindSource, MarkdownProfile::RichMarkdown);
    const auto mindDiagram        = std::ranges::find(mindDocument->Elements(), SemanticRole::Diagram, &SemanticElement::role);
    Require(mindDocument->Map().Source() == mindSource && mindDiagram != mindDocument->Elements().end() &&
                mindDiagram->name.starts_with(L"Mind map. Nodes: ") && mindDiagram->name.find(L"Central") != std::wstring::npos &&
                mindDiagram->name.find(L"First") != std::wstring::npos && mindDiagram->name.find(L"Leaf") != std::wstring::npos &&
                mindDiagram->name.find(L"Second") != std::wstring::npos,
            "Mind map semantic alternative preserves hierarchy labels and exact source");
    for (const auto* info : {L"math", L"latex"})
    {
        const auto fenceSource   = std::wstring(L"```") + info + L"\n\\frac{1}{2}\n```\n";
        const auto fenceDocument = RichSemanticDocumentTests::Build(fenceSource, MarkdownProfile::RichMarkdown);
        const auto formula       = std::ranges::find(fenceDocument->Elements(), SemanticRole::Math, &SemanticElement::role);
        Require(fenceDocument->Map().Source() == fenceSource && formula != fenceDocument->Elements().end() &&
                    fenceDocument->Map().Display().find(L'\ufffc') != std::wstring::npos && fenceDocument->Diagnostics().empty(),
                "closed display math fence prepares a native object and preserves exact source");
        Require(std::ranges::any_of(fenceDocument->Runs(),
                                    [&](const auto& run)
        { return run.start <= formula->display.first && run.start + run.length > formula->display.first && ! (run.flags & Code); }),
                "prepared math uses body typography rather than code-block style");
        const auto index = static_cast<std::size_t>(formula - fenceDocument->Elements().begin());
        Require(fenceDocument->SourceEditingNode(index).has_value() && fenceDocument->SourceEditingRange(index).has_value(),
                "math fence source edit is owned by its complete block");
    }
    const std::wstring unclosedMath = L"```math\nx + 1\n";
    const auto unclosedDocument     = RichSemanticDocumentTests::Build(unclosedMath, MarkdownProfile::RichMarkdown);
    Require(unclosedDocument->Map().Source() == unclosedMath && unclosedDocument->Map().Display().find(unclosedMath) != std::wstring::npos &&
                std::ranges::any_of(unclosedDocument->Diagnostics(),
                                    [](const auto& diagnostic) { return diagnostic.kind == SemanticDiagnosticKind::UnsupportedMath; }),
            "unclosed math fence retains its exact source and diagnostic");
    const std::wstring rejectedMath = L"```latex\n\\input{secret}\n```\n";
    const auto rejectedDocument     = RichSemanticDocumentTests::Build(rejectedMath, MarkdownProfile::RichMarkdown);
    Require(rejectedDocument->Map().Source() == rejectedMath && rejectedDocument->Map().Display().find(rejectedMath) != std::wstring::npos &&
                std::ranges::any_of(rejectedDocument->Diagnostics(),
                                    [](const auto& diagnostic) { return diagnostic.kind == SemanticDiagnosticKind::UnsupportedMath; }),
            "unsupported fenced math retains its exact source and diagnostic");
    for (const auto* displaySource : {L"$$\nx + 1\n$$\n", L"\\[\nx + 1\n\\]\n"})
    {
        const auto displayDocument = RichSemanticDocumentTests::Build(displaySource, MarkdownProfile::RichMarkdown);
        Require(displayDocument->Map().Source() == displaySource && displayDocument->Diagnostics().empty() &&
                    std::ranges::count(displayDocument->Elements(), SemanticRole::Math, &SemanticElement::role) == 1,
                "own-line paired display delimiters create one source-backed formula");
    }
    const auto inlineDisplay = RichSemanticDocumentTests::Build(L"Energy $$x$$ here\n", MarkdownProfile::RichMarkdown);
    Require(std::ranges::count(inlineDisplay->Elements(), SemanticRole::Math, &SemanticElement::role) == 0 &&
                inlineDisplay->Map().Display().find(L"$$x$$") != std::wstring::npos,
            "display delimiters embedded in a sentence remain literal");
    const std::wstring missingDisplay = L"$$\nx\n\ny\n";
    const auto missingDocument        = RichSemanticDocumentTests::Build(missingDisplay, MarkdownProfile::RichMarkdown);
    Require(missingDocument->Map().Source() == missingDisplay && missingDocument->Map().Display().find(missingDisplay) != std::wstring::npos &&
                ! missingDocument->Diagnostics().empty(),
            "unclosed display owns all remaining lines and reports a diagnostic");
    const std::wstring scopedDisplay = L"> $$\n> x\n\nafter\n";
    const auto scopedDocument        = RichSemanticDocumentTests::Build(scopedDisplay, MarkdownProfile::RichMarkdown);
    Require(scopedDocument->Map().Source() == scopedDisplay && ! scopedDocument->Diagnostics().empty() &&
                std::ranges::any_of(scopedDocument->Elements(),
                                    [](const auto& element) { return element.role == SemanticRole::Math && element.source == MarkdownRange{0u, 9u}; }) &&
                std::ranges::any_of(scopedDocument->Elements(),
                                    [](const auto& element) { return element.role == SemanticRole::Paragraph && element.source == MarkdownRange{10u, 16u}; }),
            "unclosed nested display stops at its quote boundary and leaves the following paragraph independent");
    const std::wstring listDisplay = L"- $$\n  x\n\nafter\n";
    const auto listDocument        = RichSemanticDocumentTests::Build(listDisplay, MarkdownProfile::RichMarkdown);
    Require(listDocument->Map().Source() == listDisplay && ! listDocument->Diagnostics().empty() &&
                std::ranges::any_of(listDocument->Elements(),
                                    [](const auto& element) { return element.role == SemanticRole::Math && element.source == MarkdownRange{0u, 10u}; }) &&
                std::ranges::any_of(listDocument->Elements(),
                                    [](const auto& element) { return element.role == SemanticRole::Paragraph && element.source == MarkdownRange{10u, 16u}; }),
            "unclosed nested display stops at its list boundary and leaves the following paragraph independent");
    const std::wstring styledSource = L"```mermaid\nflowchart TD\nA[\"<b>Bold</b> <i>It</i>\"]-->|<u>go</u>|B\n```\n";
    const auto styledDocument       = RichSemanticDocumentTests::Build(styledSource, MarkdownProfile::RichMarkdown);
    const auto styledDiagram        = std::ranges::find(styledDocument->Elements(), SemanticRole::Diagram, &SemanticElement::role);
    Require(styledDocument->Map().Source() == styledSource && styledDiagram != styledDocument->Elements().end() &&
                styledDiagram->name.find(L"Bold It") != std::wstring::npos && styledDiagram->name.find(L"go") != std::wstring::npos &&
                styledDiagram->name.find(L"<b>") == std::wstring::npos,
            "styled diagram accessibility exposes label text and retains exact marked-up source");
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
    const auto document = RichSemanticDocumentTests::Build(L"\ufeff---\r\n# literal **&amp;**\r\ninvalid: [\r\n---\r\n# After\r\n", MarkdownProfile::RichMarkdown);
    RichSemanticDocumentTests::CheckGraph(*document);
    Require(document->Map().Display() == L"\ufeff---\n# literal **&amp;**\ninvalid: [\n---\nAfter\n",
            "invalid metadata stays fully source-visible with no Markdown/entity decoding or fence loss");
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
            if (! IsValidSourceUtf16(cases[index]))
            {
                RichSemanticTests::Reject<std::invalid_argument>([&] { RichSemanticDocumentTests::Parse(cases[index], profile); },
                                                                 "unpaired source UTF-16 remains outside parser admission");
                ++rejectedSource;
                continue;
            }
            const auto syntax = RichSemanticDocumentTests::Parse(cases[index], profile);
            SemanticBuilder builder;
            try
            {
                builder.Begin(syntax);
                const auto document = RichSemanticDocumentTests::Complete(builder, *syntax);
                RichSemanticDocumentTests::CheckGraph(*document);
                Require(document->Map().Source() == cases[index], "pinned semantic source preserves every counted code unit including NUL");
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
void TestRichSemanticLocalizedAccessibilityLabels()
{
    RichSemanticTests::Run(RichSemanticDocumentTests::LocalizedAccessibilityLabels);
}
void RunRichSemanticDocumentTests()
{
    DXUI_RUN_TEST(TestRichSemanticLocalizedAccessibilityLabels);
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
