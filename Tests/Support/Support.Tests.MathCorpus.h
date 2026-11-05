#pragma once
#include "Controls/DxUi.NativeMathSymbols.h"
#include "Controls/DxUi.PreparedMath.h"
#include "Support.Tests.MathFontTable.h"
#include "Support.Tests.NativeMathSupportRows.h"
#include <DxUi/RichMath.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <tuple>
#include <utility>

#if DXUI_ENABLE_DIAGNOSTICS
namespace DxUi::RichText::Detail
{
extern thread_local std::uintptr_t testRichMathLowestStackAddress;
}
#endif
// Owned independent fixtures for the bounded native profile. The M01..M20
// inputs also appear in the reviewed KaTeX-0.16.11 target inventory; upstream
// TeX/HTML output is neither evaluated nor imported by the renderer.
namespace NativeMathCorpusTests
{
using namespace DxUi::RichText;
template <typename Check> inline void PinnedSupportRows(IDWriteFactory* factory, Check&& check)
{
    std::size_t parsed{}, declined{}, samples{}, previousLine{};
    for (const auto& row : NativeMathSupportRows::rows)
    {
        check(row.line > previousLine && ! row.label.empty() && ! row.source.empty(), "pinned KaTeX rows have complete ordered inventory identities");
        previousLine = row.line;
        samples += row.originalSample ? 1u : 0u;
        auto expression = std::make_shared<const MathExpression>(ParseMath(row.source));
        if (! *expression)
        {
            check(expression->issue != MathIssue::None && ! expression->diagnostic.empty() && expression->root.kind == MathKind::Row &&
                      expression->root.text.empty() && expression->root.children.empty(),
                  "declined pinned KaTeX rows publish diagnostics without a partial expression");
            ++declined;
            continue;
        }
        check(expression->issue == MathIssue::None, "admitted pinned KaTeX rows have no fallback diagnostic");
        Detail::PreparedMath layout(factory, expression, 24.f);
        std::size_t turns{};
        while (! layout.Advance())
            check(++turns <= MathMaximumNodes + 1u, "each admitted pinned KaTeX row finishes finite cooperative preparation");
        const auto size = layout.Size();
        check(std::isfinite(size.width) && std::isfinite(size.height) && size.width >= 1.f && size.height >= 1.f,
              "each admitted pinned KaTeX row publishes finite prepared bounds");
        ++parsed;
    }
    check(std::size(NativeMathSupportRows::rows) == 1135u && samples == 995u && parsed + declined == 1135u,
          "every pinned KaTeX support row runs, including 140 explicitly identified command-only probes");
    std::cout << "Pinned KaTeX support rows: 1135; original samples=" << samples << "; parsed=" << parsed << "; declined=" << declined << '\n';
}
struct Scenario
{
    std::wstring_view id, source;
};
inline constexpr Scenario prepared[] = {
    {L"M01", L"a+12.5=b"},
    {L"M02", L"{a+{b-c}}"},
    {L"M03", L"\\frac{a+b}{c}"},
    {L"M04", L"\\dfrac{1}{\\tfrac{x}{2}}"},
    {L"M05", L"\\sqrt{x}"},
    {L"M06", L"\\sqrt[3]{x+1}"},
    {L"M07", L"x_i^{n+1}"},
    {L"M08", L"x^2_i"},
    {L"M09", L"\\left(\\frac{a}{b}\\right]"},
    {L"M10", L"\\left. x \\right|_0^1"},
    {L"M11", L"\\hat{x}+\\overline{a+b}+\\vec{v}"},
    {L"M12", L"\\sum_{i=1}^{n}i^2"},
    {L"M13", L"\\int\\limits_0^1 x\\,dx"},
    {L"M14", L"\\begin{pmatrix}a&b\\\\c&d\\end{pmatrix}"},
    {L"M15", L"\\begin{bmatrix}a&b\\\\c&\\end{bmatrix}"},
    {L"M16", L"\\begin{cases}x^2&x>0\\\\0&\\text{otherwise}\\end{cases}"},
    {L"M17", L"\\begin{aligned}a&=b+c\\\\d&=e\\end{aligned}"},
    {L"M18", L"\\text{Unicode \u03b2 \U0001f600}+\\mathrm{speed}"},
    {L"M19", L"\\alpha\\leq\\infty"},
    {L"M20", L"\\widehat{ABC}+\\ddot{x}+\\underline{y}"},
    {L"binomial", L"\\binom{n}{k}+\\dbinom{a+b}{2}+\\tbinom{x}{y}"},
    {L"over under", L"\\overset{a}{=}+\\underset{b}{x}+\\stackrel{c}{=}"},
    {L"styles", L"\\displaystyle\\sum_{i=0}^n\\frac{x_i}{2}+{\\scriptstyle x^{y^z}}"},
    {L"limits after scripts", L"\\sum_0^1\\nolimits+\\operatorname*{argmax}\\limits_x"},
    {L"sized delimiters", L"\\bigl( x \\Bigr]+\\biggl\\lfloor x\\Biggr\\rceil"},
    {L"tilde accents", L"\\tilde{x}+\\widetilde{ABC}"},
    {L"text whitespace", L"\\text{speed\r\n in\t {m/s}}"},
    {L"TeX comments", L"x% a source comment\r\n+y"},
    {L"matrix trailing row", L"\\begin{matrix}a&b\\\\c&d\\\\\\end{matrix}"},
    {L"small matrix", L"\\begin{smallmatrix}a&b\\\\c&d\\end{smallmatrix}"},
    {L"gathered", L"\\begin{gathered}a=b\\\\c=d\\end{gathered}"},
    {L"atom overrides", L"a\\mathbin{*}b\\mathrel{=}c+\\mathop{arg}\\limits_x"},
};
inline constexpr Scenario rejected[] = {
    {L"row separator outside environment", L"a\\\\b"},
    {L"E01 missing fraction argument", L"\\frac{a}"},
    {L"missing binomial argument", L"\\binom{n}"},
    {L"missing over argument", L"\\overset{x}"},
    {L"E02 missing index close", L"\\sqrt[3{x}"},
    {L"E03 missing script argument", L"x_"},
    {L"E04 duplicate subscript", L"x_1_2"},
    {L"E05 missing group close", L"{x"},
    {L"E06 missing right delimiter", L"\\left(x"},
    {L"E07 environment mismatch", L"\\begin{matrix}a\\end{cases}"},
    {L"E08 separator outside environment", L"a&b"},
    {L"unsafe macro", L"\\newcommand{\\foo}{x}"},
    {L"unsafe file input", L"\\input{local}"},
    {L"unknown command", L"\\unknown{x}"},
    {L"unsupported package", L"\\usepackage{amsmath}"},
    {L"unsupported execution", L"\\write18{command}"},
    {L"unsupported environment", L"\\begin{CD}a&b\\end{CD}"},
    {L"standalone modifier", L"\\limits x"},
    {L"ordinary modifier", L"x\\nolimits_2"},
    {L"duplicate modifier", L"\\sum\\limits\\nolimits_2"},
    {L"duplicate superscript", L"x^2^3"},
    {L"gathered columns", L"\\begin{gathered}a&b\\\\\\end{gathered}"},
    {L"cases columns", L"\\begin{cases}a&b&c\\\\\\end{cases}"},
};
struct Symbol
{
    std::wstring_view command, value;
};
inline constexpr Symbol symbols[] = {
    {L"alpha", L"α"},    {L"beta", L"β"},    {L"gamma", L"γ"},  {L"delta", L"δ"},   {L"epsilon", L"ϵ"},  {L"varepsilon", L"ε"}, {L"theta", L"θ"},
    {L"vartheta", L"ϑ"}, {L"phi", L"ϕ"},     {L"varphi", L"φ"}, {L"Upsilon", L"Υ"}, {L"cdot", L"⋅"},     {L"ell", L"ℓ"},        {L"hbar", L"ℏ"},
    {L"varkappa", L"ϰ"}, {L"omicron", L"ο"}, {L"varpi", L"ϖ"},  {L"varrho", L"ϱ"},  {L"varsigma", L"ς"}, {L"lvert", L"|"},      {L"rvert", L"|"},
    {L"lVert", L"‖"},    {L"rVert", L"‖"},   {L"lfloor", L"⌊"}, {L"rfloor", L"⌋"},  {L"lceil", L"⌈"},    {L"rceil", L"⌉"},      {L"coprod", L"∐"},
    {L"bigcup", L"⋃"},   {L"bigcap", L"⋂"},  {L"sec", L"sec"},  {L"csc", L"csc"},
};
// Canonical structural oracle: retain every node, ordered child, payload and matrix width.
inline std::wstring Shape(const MathNode& node)
{
    constexpr wchar_t kinds[] = {L'R', L'T', L'F', L'Q', L'S', L'M', L'A', L'Y', L'P', L'D', L'L'};
    std::wstring result(1u, kinds[static_cast<std::size_t>(node.kind)]);
    if (! node.text.empty())
        result += L"[" + node.text + L"]";
    if (node.kind == MathKind::Matrix)
        result += L"#" + std::to_wstring(node.columns);
    if (! node.children.empty())
    {
        result += L'(';
        for (std::size_t index = 0u; index < node.children.size(); ++index)
        {
            if (index)
                result += L',';
            result += Shape(node.children[index]);
        }
        result += L')';
    }
    return result;
}
inline constexpr std::wstring_view targetShapes[] = {
    L"R(T[a],T[+],T[12.5],T[=],T[b])",
    L"R(R(T[a],T[+],R(T[b],T[−],T[c])))",
    L"R(F[frac](R(T[a],T[+],T[b]),R(T[c])))",
    L"R(F[dfrac](R(T[1]),R(F[tfrac](R(T[x]),R(T[2])))))",
    L"R(Q(R(T[x]),R))",
    L"R(Q(R(T[x],T[+],T[1]),R(T[3])))",
    L"R(S(T[x],T[i],R(T[n],T[+],T[1])))",
    L"R(S(T[x],T[i],T[2]))",
    L"R(D[(]](R(F[frac](R(T[a]),R(T[b])))))",
    L"R(S(D[.|](R(T[x])),T[0],T[1]))",
    L"R(A[hat](R(T[x])),T[+],A[overline](R(T[a],T[+],T[b])),T[+],A[vec](R(T[v])))",
    L"R(S(T[∑],R(T[i],T[=],T[1]),R(T[n])),S(T[i],R,T[2]))",
    L"R(S(T[∫],T[0],T[1]),T[x],P[,],T[dx])",
    L"R(M[pmatrix]#2(R(T[a]),R(T[b]),R(T[c]),R(T[d])))",
    L"R(M[bmatrix]#2(R(T[a]),R(T[b]),R(T[c]),R))",
    L"R(M[cases]#2(R(S(T[x],R,T[2])),R(T[x],T[>],T[0]),R(T[0]),R(T[otherwise])))",
    L"R(M[aligned]#2(R(T[a]),R(T[=],T[b],T[+],T[c]),R(T[d]),R(T[=],T[e])))",
    L"R(T[Unicode β 😀],T[+],Y[mathrm](R(T[speed])))",
    L"R(T[α],T[≤],T[∞])",
    L"R(A[widehat](R(T[ABC])),T[+],A[ddot](R(T[x])),T[+],A[underline](R(T[y])))",
};
inline std::wstring Matrix(unsigned rows, unsigned columns)
{
    std::wstring source = L"\\begin{matrix}";
    for (unsigned row = 0u; row < rows; ++row)
    {
        if (row)
            source += L"\\\\";
        for (unsigned column = 0u; column < columns; ++column)
        {
            if (column)
                source += L'&';
            source += L'a';
        }
    }
    return source + L"\\end{matrix}";
}
template <typename Glyph, typename Check> auto GlyphInkBounds(const Glyph& glyph, Check&& check)
{
    DWRITE_OVERHANG_METRICS overhang{};
    DWRITE_TEXT_METRICS metrics{};
    check(SUCCEEDED(glyph.layout->GetOverhangMetrics(&overhang)) && SUCCEEDED(glyph.layout->GetMetrics(&metrics)),
          "the independent ink oracle reads actual shaped DirectWrite metrics");
    // Overhang is relative to the configured layout rectangle. The glyph
    // line height is smaller and would undercount descenders/subscripts.
    return std::pair{glyph.origin.y - overhang.top, glyph.origin.y + metrics.layoutHeight + overhang.bottom + (glyph.pseudoBold ? glyph.font * .01f : 0.f)};
}
template <typename Check> void AccentAndGatheredGeometry(IDWriteFactory* factory, Check&& check)
{
    const auto complete = [&](Detail::PreparedMath& math)
    {
        std::size_t turns{};
        while (! math.Advance())
            check(++turns < MathMaximumNodes + 1u, "reported mathematical regressions finish bounded preparation");
    };
    const auto glyphBounds = [&](const auto& glyph) { return GlyphInkBounds(glyph, check); };
    for (const bool display : {false, true})
    {
        for (const auto name : {L"aligned", L"gathered"})
        {
            const auto source = std::wstring(L"\\begin{") + name + L"}\\frac{a}{b}\\end{" + name + L"}";
            Detail::PreparedMath math(factory, std::make_shared<const MathExpression>(ParseMath(source)), 24.f, display);
            complete(math);
            check(math.GlyphLayouts().size() == 2u &&
                      std::ranges::all_of(math.GlyphLayouts(), [](const auto& glyph) { return std::abs(glyph.font - 24.f) < .001f; }),
                  "aligned and gathered fractions use display-style cells independently of the surrounding style");
        }
        Detail::PreparedMath stacked(factory, std::make_shared<const MathExpression>(ParseMath(L"\\substack{a\\\\b}")), 24.f, display);
        Detail::PreparedMath script(factory, std::make_shared<const MathExpression>(ParseMath(L"\\scriptstyle a")), 24.f, display);
        complete(stacked);
        complete(script);
        check(stacked.GlyphLayouts().size() == 2u && script.GlyphLayouts().size() == 1u &&
                  std::ranges::all_of(stacked.GlyphLayouts(), [&](const auto& glyph) { return std::abs(glyph.font - script.GlyphLayouts()[0u].font) < .001f; }),
              "substack stays a centered script-style subarray when gathered cells gain display style");
    }
    for (const auto family : {L"Cambria Math", L"Segoe UI"})
        for (const float font : {16.f, 24.f, 48.f})
            for (const float dpi : {96.f, 144.f, 192.f})
                for (const auto source :
                     {L"\\widehat{ABC}", L"\\widetilde{XYZ}", L"\\overline{a+b}", L"\\underline{x_i}", L"\\vec{v}", L"\\dot{x}", L"\\ddot{x}"})
                {
                    const auto expression = std::make_shared<const MathExpression>(ParseMath(source));
                    check(static_cast<bool>(*expression), "the reported accent sources are admitted exactly");
                    auto formats = std::make_shared<Detail::MathTextFormats>(factory, family);
                    Detail::PreparedMath math(factory, expression, font, false, nullptr, dpi, std::move(formats));
                    complete(math);
                    float textTop = std::numeric_limits<float>::max(), textBottom = -textTop;
                    for (const auto& glyph : math.GlyphLayouts())
                    {
                        const auto [top, bottom] = glyphBounds(glyph);
                        textTop                  = std::min(textTop, top);
                        textBottom               = std::max(textBottom, bottom);
                    }
                    float markTop = std::numeric_limits<float>::max(), markBottom = -markTop;
                    for (const auto& line : math.StrokeLayouts())
                    {
                        markTop    = std::min(markTop, std::min(line.first.y, line.last.y) - line.thickness * .5f);
                        markBottom = std::max(markBottom, std::max(line.first.y, line.last.y) + line.thickness * .5f);
                        if (line.dot)
                            check(line.thickness >= std::max(1.4f, font * .10f), "math dots retain a visible filled diameter at every size and DPI");
                    }
                    for (const auto& variant : math.VariantLayouts())
                    {
                        markTop    = std::min(markTop, variant.top);
                        markBottom = std::max(markBottom, variant.top + variant.height);
                    }
                    const bool below = std::wstring_view(source).starts_with(L"\\underline");
                    const auto gap   = below ? markTop - textBottom : textTop - markBottom;
                    if (! std::isfinite(gap) || gap < font * .025f || gap > font * .18f)
                        std::wcerr << L"Accent geometry: " << source << L" family=" << family << L" font=" << font << L" dpi=" << dpi << L" textTop=" << textTop
                                   << L" textBottom=" << textBottom << L" markTop=" << markTop << L" markBottom=" << markBottom << L" gap=" << gap << L'\n';
                    check(std::isfinite(gap) && gap >= font * .025f && gap <= font * .18f,
                          "accent ink stays close to the actual letters with a remaining non-touching gap");
                    if (std::wstring_view(source).starts_with(L"\\widetilde") && math.VariantLayouts().empty())
                    {
                        unsigned turns{};
                        int previous{};
                        for (const auto& line : math.StrokeLayouts())
                        {
                            const auto delta = line.last.y - line.first.y;
                            if (std::abs(delta) <= font * .0001f)
                                continue;
                            const int direction = delta > 0.f ? 1 : -1;
                            turns += previous && previous != direction;
                            previous = direction;
                        }
                        check(turns == 2u, "a fallback wide tilde has one smooth crest and trough rather than a repeated zigzag");
                    }
                }
    constexpr auto gathered = L"\\begin{gathered}\\mathbf{u}=\\begin{pmatrix}u_1\\\\u_2\\end{pmatrix},\\quad"
                              L"\\mathbf{v}=\\begin{pmatrix}v_1\\\\v_2\\end{pmatrix}\\\\"
                              L"\\left\\langle\\mathbf{u},\\mathbf{v}\\right\\rangle=u_1v_1+u_2v_2\\end{gathered}";
    for (const bool display : {false, true})
        for (const float font : {16.f, 24.f, 48.f})
        {
            const auto expression = std::make_shared<const MathExpression>(ParseMath(gathered));
            check(*expression && expression->root.children.size() == 1u && expression->root.children[0u].kind == MathKind::Matrix &&
                      expression->root.children[0u].children.size() == 2u,
                  "the reported gathered vectors retain two exact source-owned rows");
            const auto& rows    = expression->root.children[0u].children;
            const auto contains = [](const auto& self, const MathNode& node, const MathNode* target) -> bool
            {
                if (&node == target)
                    return true;
                return std::ranges::any_of(node.children, [&](const auto& child) { return self(self, child, target); });
            };
            Detail::PreparedMath math(factory, expression, font, display);
            complete(math);
            float firstBottom = -std::numeric_limits<float>::max(), secondTop = -firstBottom;
            for (const auto& glyph : math.GlyphLayouts())
            {
                const auto [top, bottom] = glyphBounds(glyph);
                if (contains(contains, rows[0u], glyph.node))
                    firstBottom = std::max(firstBottom, bottom);
                else if (contains(contains, rows[1u], glyph.node))
                    secondTop = std::min(secondTop, top);
            }
            check(secondTop - firstBottom >= font * .12f && math.Size().height < font * 6.f,
                  "nested matrices use compact rows and leave a real gap before the inner-product equation");
            const auto between = (firstBottom + secondTop) * .5f;
            for (const auto& variant : math.VariantLayouts())
            {
                if (! (variant.top + variant.height < between || variant.top > between))
                    std::cerr << "Gathered delimiter: font=" << font << " display=" << display << " firstBottom=" << firstBottom << " secondTop=" << secondTop
                              << " between=" << between << " top=" << variant.top << " height=" << variant.height << " glyph=" << variant.glyph << '\n';
                check(variant.top + variant.height < between || variant.top > between,
                      "native matrix and angle delimiter ink never spans the gap between gathered rows");
            }
            for (const auto& stroke : math.StrokeLayouts())
                check(std::max(stroke.first.y, stroke.last.y) + stroke.thickness * .5f < between ||
                          std::min(stroke.first.y, stroke.last.y) - stroke.thickness * .5f > between,
                      "fallback delimiter strokes never cross the gathered-row gap");
        }
}
template <typename Check> void Run(IDWriteFactory* factory, Check&& check)
{
    AccentAndGatheredGeometry(factory, check);
    PinnedSupportRows(factory, check);
    MathFontTableTests::Run(check);
    {
        const auto source     = L"\\mathbb{\\text{" + std::wstring(8200u, L'A') + L"}}";
        const auto expression = std::make_shared<const MathExpression>(ParseMath(source));
        check(static_cast<bool>(*expression), "authored blackboard source below the source cap parses before expansion");
        Detail::PreparedMath expanded(factory, expression, 1.f);
        while (! expanded.Advance())
        {
        }
        check(expanded.Size().width > 0.f, "bounded surrogate expansion does not reject a valid authored source");
        auto oversized = ParseMath(L"\\text{a}");
        oversized.root.children[0u].text.assign(2u * MathMaximumSourceUnits + 1u, L'a');
        bool refused = false;
        try
        {
            Detail::PreparedMath invalid(factory, std::make_shared<const MathExpression>(std::move(oversized)), 1.f);
        }
        catch (const std::length_error&)
        {
            refused = true;
        }
        check(refused, "direct mathematical text payloads remain bounded after allowing alphabet expansion");
    }
    {
        const auto dots    = std::make_shared<const MathExpression>(ParseMath(L"a\\ldots b"));
        const auto literal = std::make_shared<const MathExpression>(ParseMath(L"a…b"));
        check(*dots && dots->root.children.size() == 3u && dots->root.children[1u].atomClass == MathAtomClass::Inner && *literal &&
                  literal->root.children.size() == 3u && literal->root.children[1u].atomClass == MathAtomClass::Ordinary,
              "ldots is Inner while the authored literal ellipsis remains Ordinary");
        Detail::PreparedMath withDots(factory, dots, 24.f), withLiteral(factory, literal, 24.f);
        while (! withDots.Advance())
        {
        }
        while (! withLiteral.Advance())
        {
        }
        check(std::abs((withDots.Size().width - withLiteral.Size().width) - 8.f) < .01f,
              "ldots adds two TeX thin spaces around the same independently shaped glyphs");
    }
    check(Detail::ParseNativeColor(L"#AbC") == 0xaabbccu && Detail::ParseNativeColor(L"rebeccaPurple") == 0x663399u &&
              Detail::ParseNativeColor(L"DarkGrey") == 0xa9a9a9u && ! Detail::ParseNativeColor(L"#xyz") && ! Detail::ParseNativeColor(L"#12345678"),
          "native color admission handles named colors, case, short hex and rejects unsupported alpha or malformed input");
    for (const auto source : {L"a\\textcolor{blue}{+}b",
                              L"\\color{blue}{2+}3",
                              L"\\colorbox{black}{plain text}",
                              L"\\fcolorbox{white}{black}{plain text}",
                              L"\\textcolor{red}{x_1}",
                              L"\\colorbox{black}{\\textcolor{white}{hello world}}",
                              L"\\colorbox{blue}{cost \\$5}"})
        check(static_cast<bool>(ParseMath(source)), "bounded author-color syntax parses");
    for (const auto source : {L"\\color{unknownname}x", L"\\textcolor{#12345678}{x}", L"\\colorbox{blue}", L"\\fcolorbox{red}{blue}"})
        check(! ParseMath(source), "malformed or unsupported author colors reject to source");
    const auto compareColoredSpacing = [&](std::wstring_view source, std::wstring_view expected)
    {
        const auto left  = std::make_shared<const MathExpression>(ParseMath(source));
        const auto right = std::make_shared<const MathExpression>(ParseMath(expected));
        check(static_cast<bool>(*left) && static_cast<bool>(*right), "colored spacing fixtures parse");
        Detail::PreparedMath colored(factory, left, 24.f), plain(factory, right, 24.f);
        while (! colored.Advance())
        {
        }
        while (! plain.Advance())
        {
        }
        check(std::abs(colored.Size().width - plain.Size().width) < .01f && colored.GlyphLayouts().size() == plain.GlyphLayouts().size(),
              "color wrappers retain uncolored TeX atom spacing");
        if (colored.GlyphLayouts().size() == plain.GlyphLayouts().size())
            for (std::size_t i = 0u; i < colored.GlyphLayouts().size(); ++i)
                check(std::abs(colored.GlyphLayouts()[i].origin.x - plain.GlyphLayouts()[i].origin.x) < .01f,
                      "colored atoms retain their uncolored SDK origin");
    };
    compareColoredSpacing(L"a\\textcolor{blue}{+}b", L"a+b");
    compareColoredSpacing(L"a\\textcolor{blue}{{+}}b", L"a{{+}}b");
    compareColoredSpacing(L"\\color{blue}{2+}3", L"{2+}3");
    compareColoredSpacing(L"a\\color{blue}{+}b", L"a{+}b");
    compareColoredSpacing(L"a\\color{blue}{{+}}b", L"a{{+}}b");
    std::wstring nestedColor(0u, L' ');
    for (std::size_t depth = 0u; depth < MathMaximumGroupDepth; ++depth)
        nestedColor.append(L"\\colorbox{white}{");
    nestedColor += L'x';
    nestedColor.append(MathMaximumGroupDepth, L'}');
    check(static_cast<bool>(ParseMath(nestedColor)), "author-color text groups admit the published nesting boundary");
    check(Detail::OptionalMathFontOperation(S_OK) && ! Detail::OptionalMathFontOperation(DWRITE_E_UNSUPPORTEDOPERATION),
          "optional math font operation falls back only for unsupported operations");
    bool outOfMemoryPropagated = false;
    try
    {
        static_cast<void>(Detail::OptionalMathFontOperation(E_OUTOFMEMORY));
    }
    catch (const wil::ResultException& error)
    {
        outOfMemoryPropagated = error.GetErrorCode() == E_OUTOFMEMORY;
    }
    check(outOfMemoryPropagated, "optional math font operation propagates allocation failure");
#if DXUI_ENABLE_DIAGNOSTICS
    {
        wil::com_ptr<IDWriteFontCollection> collection;
        THROW_IF_FAILED(factory->GetSystemFontCollection(collection.put()));
        UINT32 familyIndex{};
        BOOL found{};
        THROW_IF_FAILED(collection->FindFamilyName(L"Cambria Math", &familyIndex, &found));
        check(found, "SDK math-kern oracle requires the system math family");
        wil::com_ptr<IDWriteFontFamily> family;
        THROW_IF_FAILED(collection->GetFontFamily(familyIndex, family.put()));
        wil::com_ptr<IDWriteFont> font;
        THROW_IF_FAILED(family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, font.put()));
        wil::com_ptr<IDWriteFontFace> face;
        THROW_IF_FAILED(font->CreateFontFace(face.put()));
        DWRITE_FONT_METRICS fontMetrics{};
        face->GetMetrics(&fontMetrics);
        {
            const void* bytes{};
            UINT32 byteCount{};
            void* context{};
            BOOL exists{};
            THROW_IF_FAILED(face->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('M', 'A', 'T', 'H'), &bytes, &byteCount, &context, &exists));
            const auto releaseTable = wil::scope_exit([&]() noexcept
            {
                if (context)
                    face->ReleaseFontTable(context);
            });
            check(exists && bytes && fontMetrics.designUnitsPerEm, "SDK shift oracle has the actual system MATH table");
            if (exists && bytes && fontMetrics.designUnitsPerEm)
            {
                const Detail::MathFontTable table(std::span(static_cast<const std::uint8_t*>(bytes), byteCount));
                const auto record   = table.MathConstant(36u);
                const auto minShift = record ? table.AdjustedValue(record.value(), 24.f, 96.f, fontMetrics.designUnitsPerEm) : std::nullopt;
                check(minShift.has_value(), "SDK MATH table supplies the superscript minimum shift");
                for (const auto source : {L"H^x", L".^x"})
                {
                    auto expression = std::make_shared<const MathExpression>(ParseMath(source));
                    Detail::PreparedMath scriptInkProbe(factory, expression, 24.f);
                    while (! scriptInkProbe.Advance())
                    {
                    }
                    const auto& glyphs = scriptInkProbe.GlyphLayouts();
                    check(glyphs.size() == 2u, "SDK shift fixture shapes a base and superscript");
                    if (glyphs.size() != 2u || ! minShift)
                        continue;
                    DWRITE_LINE_METRICS line{};
                    UINT32 count{};
                    DWRITE_OVERHANG_METRICS ink{};
                    THROW_IF_FAILED(glyphs[0u].layout->GetLineMetrics(&line, 1u, &count));
                    THROW_IF_FAILED(glyphs[0u].layout->GetOverhangMetrics(&ink));
                    const float expected = std::max(minShift.value(), std::max(0.f, line.baseline + ink.top) - 24.f * .08f);
                    const float actual   = glyphs[0u].origin.y + glyphs[0u].extent.above - glyphs[1u].origin.y - glyphs[1u].extent.above;
                    check(std::abs(actual - expected) < .05f, "superscript baseline uses actual glyph ink and SDK MATH minimum");
                }
                for (const auto source : {L"{.+.}^x", L"{H_i}^x", L"{\\frac{.}{.}}^x"})
                {
                    auto compositeExpression = std::make_shared<const MathExpression>(ParseMath(source));
                    Detail::PreparedMath composite(factory, compositeExpression, 24.f);
                    while (! composite.Advance())
                    {
                    }
                    const auto& glyphs = composite.GlyphLayouts();
                    check(glyphs.size() >= 3u && minShift, "compound shift oracle has a shaped base and superscript");
                    if (glyphs.size() < 3u || ! minShift)
                        continue;
                    float top = composite.Baseline();
                    for (std::size_t index = 0u; index + 1u < glyphs.size(); ++index)
                    {
                        DWRITE_OVERHANG_METRICS ink{};
                        THROW_IF_FAILED(glyphs[index].layout->GetOverhangMetrics(&ink));
                        top = std::min(top, glyphs[index].origin.y - ink.top);
                    }
                    for (const auto& stroke : composite.StrokeLayouts())
                        top = std::min(top, std::min(stroke.first.y, stroke.last.y) - stroke.thickness * .5f);
                    const float expected = std::max(minShift.value(), composite.Baseline() - top - 24.f * .08f);
                    const auto& upper    = glyphs.back();
                    const float actual   = composite.Baseline() - upper.origin.y - upper.extent.above;
                    check(std::abs(actual - expected) < .05f, "row, nested script and fraction bases use their actual staged ink union");
                }
                const auto constant = [&](std::size_t offset) -> std::optional<float>
                {
                    const auto entry = table.MathConstant(offset);
                    return entry ? table.AdjustedValue(entry.value(), 24.f, 96.f, fontMetrics.designUnitsPerEm) : std::nullopt;
                };
                const auto axis = constant(12u), denominator = constant(128u), gap = constant(148u), rule = constant(144u);
                check(axis && denominator && gap && rule, "SDK MATH table supplies fraction constants");
                auto fractionExpression = std::make_shared<const MathExpression>(ParseMath(L"\\frac{H}{.}"));
                Detail::PreparedMath fraction(factory, fractionExpression, 24.f);
                while (! fraction.Advance())
                {
                }
                const auto& fractionGlyphs = fraction.GlyphLayouts();
                check(fractionGlyphs.size() == 2u, "SDK fraction oracle shapes both operands");
                if (fractionGlyphs.size() == 2u && axis && denominator && gap && rule)
                {
                    DWRITE_LINE_METRICS line{};
                    UINT32 count{};
                    DWRITE_OVERHANG_METRICS ink{};
                    THROW_IF_FAILED(fractionGlyphs[1u].layout->GetLineMetrics(&line, 1u, &count));
                    THROW_IF_FAILED(fractionGlyphs[1u].layout->GetOverhangMetrics(&ink));
                    const float expected =
                        std::max(denominator.value(), std::max(0.f, line.baseline + ink.top) + gap.value() + std::max(1.f, rule.value()) * .5f - axis.value());
                    const float actual = fractionGlyphs[1u].origin.y + fractionGlyphs[1u].extent.above - fraction.Baseline();
                    check(std::abs(actual - expected) < .05f, "fraction denominator uses actual glyph ink and SDK MATH gaps");
                }
                auto rowFractionExpression = std::make_shared<const MathExpression>(ParseMath(L"\\frac{H}{.+.}"));
                Detail::PreparedMath rowFraction(factory, rowFractionExpression, 24.f);
                while (! rowFraction.Advance())
                {
                }
                const auto& rowGlyphs = rowFraction.GlyphLayouts();
                check(rowGlyphs.size() == 4u, "row denominator shapes three independent atoms");
                if (rowGlyphs.size() == 4u && axis && denominator && gap && rule)
                {
                    float inkAbove{};
                    for (std::size_t index = 1u; index < rowGlyphs.size(); ++index)
                    {
                        DWRITE_LINE_METRICS line{};
                        DWRITE_OVERHANG_METRICS ink{};
                        UINT32 count{};
                        THROW_IF_FAILED(rowGlyphs[index].layout->GetLineMetrics(&line, 1u, &count));
                        THROW_IF_FAILED(rowGlyphs[index].layout->GetOverhangMetrics(&ink));
                        inkAbove = std::max(inkAbove, line.baseline + ink.top);
                    }
                    const float expected = std::max(denominator.value(), inkAbove + gap.value() + std::max(1.f, rule.value()) * .5f - axis.value());
                    const float actual   = rowGlyphs[1u].origin.y + rowGlyphs[1u].extent.above - rowFraction.Baseline();
                    check(std::abs(actual - expected) < .05f, "multi-atom denominator uses the union of actual SDK ink");
                }
                auto pairedExpression = std::make_shared<const MathExpression>(ParseMath(L"H_i^x"));
                Detail::PreparedMath paired(factory, pairedExpression, 24.f);
                while (! paired.Advance())
                {
                }
                const auto& pairedGlyphs = paired.GlyphLayouts();
                const auto scriptGap     = constant(52u);
                check(pairedGlyphs.size() == 3u && scriptGap, "paired-script SDK oracle has both scripts and a minimum gap");
                if (pairedGlyphs.size() == 3u && scriptGap)
                {
                    DWRITE_OVERHANG_METRICS subInk{}, superInk{};
                    THROW_IF_FAILED(pairedGlyphs[1u].layout->GetOverhangMetrics(&subInk));
                    THROW_IF_FAILED(pairedGlyphs[2u].layout->GetOverhangMetrics(&superInk));
                    const float subTop      = pairedGlyphs[1u].origin.y - subInk.top;
                    const float superBottom = pairedGlyphs[2u].origin.y + pairedGlyphs[2u].layout->GetMaxHeight() + superInk.bottom;
                    check(subTop - superBottom + .05f >= scriptGap.value(), "paired scripts preserve the SDK MATH ink gap");
                }
            }
        }
        const auto strikeExpression = std::make_shared<const MathExpression>(ParseMath(L"\\sout{x}"));
        Detail::PreparedMath strike(factory, strikeExpression, 24.f);
        while (! strike.Advance())
        {
        }
        const auto expectedStrike = strike.Baseline() - 12.f * static_cast<float>(fontMetrics.xHeight) / static_cast<float>(fontMetrics.designUnitsPerEm);
        check(strike.StrokeLayouts().size() == 1u && std::abs(strike.StrokeLayouts()[0].first.y - expectedStrike) < .01f,
              "horizontal strike uses the actual SDK font x-height about its baseline");
        const UINT32 scalar = L'1';
        UINT16 glyph{};
        THROW_IF_FAILED(face->GetGlyphIndices(&scalar, 1u, &glyph));
        check(glyph != 0u && fontMetrics.designUnitsPerEm != 0u, "SDK reports the actual math glyph and design scale");
        check(Detail::SameMathFontFile(face.get(), face.get()), "same real SDK font identity accepts its own MATH table");
        UINT32 sourceFileCount{};
        THROW_IF_FAILED(face->GetFiles(&sourceFileCount, nullptr));
        check(sourceFileCount == 1u, "math font identity fixture has exactly one source file");
        wil::com_ptr<IDWriteFontFile> sourceFile;
        THROW_IF_FAILED(face->GetFiles(&sourceFileCount, sourceFile.put()));
        auto* sourceFilePointer = sourceFile.get();
        wil::com_ptr<IDWriteFontFace> simulated;
        THROW_IF_FAILED(factory->CreateFontFace(face->GetType(), 1u, &sourceFilePointer, face->GetIndex(), DWRITE_FONT_SIMULATIONS_OBLIQUE, simulated.put()));
        check(! Detail::SameMathFontFile(face.get(), simulated.get()), "the same SDK font file with synthetic oblique cannot borrow normal-face MATH kerning");
        std::array<wchar_t, MAX_PATH> windowsDirectory{};
        const UINT windowsLength = GetWindowsDirectoryW(windowsDirectory.data(), static_cast<UINT>(windowsDirectory.size()));
        check(windowsLength > 0u && windowsLength < windowsDirectory.size(), "variable font fixture resolves the Windows font directory");
        std::wstring variablePath(windowsDirectory.data(), windowsLength);
        variablePath.append(L"\\Fonts\\SegUIVar.ttf");
        wil::com_ptr<IDWriteFontFile> variableFile;
        THROW_IF_FAILED(factory->CreateFontFileReference(variablePath.c_str(), nullptr, variableFile.put()));
        BOOL variableSupported{};
        DWRITE_FONT_FILE_TYPE variableFileType{};
        DWRITE_FONT_FACE_TYPE variableFaceType{};
        UINT32 variableFaces{};
        THROW_IF_FAILED(variableFile->Analyze(&variableSupported, &variableFileType, &variableFaceType, &variableFaces));
        check(variableSupported && variableFaces > 0u, "variable font fixture opens a supported system font");
        auto* variableFilePointer = variableFile.get();
        wil::com_ptr<IDWriteFontFace> variableFace;
        THROW_IF_FAILED(factory->CreateFontFace(variableFaceType, 1u, &variableFilePointer, 0u, DWRITE_FONT_SIMULATIONS_NONE, variableFace.put()));
        wil::com_ptr<IDWriteFontFace5> variableFace5;
        THROW_IF_FAILED(variableFace->QueryInterface(IID_PPV_ARGS(variableFace5.put())));
        wil::com_ptr<IDWriteFontResource> variableResource;
        THROW_IF_FAILED(variableFace5->GetFontResource(variableResource.put()));
        const auto variableCount = variableResource->GetFontAxisCount();
        check(variableCount > 0u && variableCount <= 64u, "variable font fixture exposes bounded axes");
        std::array<DWRITE_FONT_AXIS_VALUE, 64u> variableAxes{};
        std::array<DWRITE_FONT_AXIS_RANGE, 64u> variableRanges{};
        THROW_IF_FAILED(variableResource->GetDefaultFontAxisValues(variableAxes.data(), variableCount));
        THROW_IF_FAILED(variableResource->GetFontAxisRanges(variableRanges.data(), variableCount));
        wil::com_ptr<IDWriteFontFace5> defaultInstance, alternateInstance;
        THROW_IF_FAILED(variableResource->CreateFontFace(DWRITE_FONT_SIMULATIONS_NONE, variableAxes.data(), variableCount, defaultInstance.put()));
        bool changedAxis{};
        for (UINT32 axis = 0u; axis < variableCount; ++axis)
            if (variableRanges[axis].maxValue > variableRanges[axis].minValue)
            {
                variableAxes[axis].value =
                    variableAxes[axis].value == variableRanges[axis].maxValue ? variableRanges[axis].minValue : variableRanges[axis].maxValue;
                changedAxis = true;
                break;
            }
        check(changedAxis, "variable font fixture supplies a nonconstant axis");
        THROW_IF_FAILED(variableResource->CreateFontFace(DWRITE_FONT_SIMULATIONS_NONE, variableAxes.data(), variableCount, alternateInstance.put()));
        check(Detail::SameMathFontFile(defaultInstance.get(), defaultInstance.get()) &&
                  ! Detail::SameMathFontFile(defaultInstance.get(), alternateInstance.get()),
              "same-file variable font instances match only at identical canonical axes");
        wil::com_ptr<IDWriteTextFormat> fallbackFormat;
        THROW_IF_FAILED(factory->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.f, L"en-us", fallbackFormat.put()));
        wil::com_ptr<IDWriteTextLayout> fallbackLayout;
        THROW_IF_FAILED(factory->CreateTextLayout(L"x", 1u, fallbackFormat.get(), 100.f, 100.f, fallbackLayout.put()));
        const auto fallbackProbe = Microsoft::WRL::Make<Detail::MathGlyphProbe>();
        check(static_cast<bool>(fallbackProbe), "SDK fallback font probe allocates");
        THROW_IF_FAILED(fallbackLayout->Draw(nullptr, fallbackProbe.Get(), 0.f, 0.f));
        check(fallbackProbe->runs == 1u && fallbackProbe->face && ! Detail::SameMathFontFile(face.get(), fallbackProbe->face.get()),
              "a differently resolved SDK font face cannot borrow math kern data from Cambria Math");
        std::array<std::uint8_t, 256u> data{};
        const auto write = [&](std::size_t at, std::uint16_t value)
        {
            data[at]      = static_cast<std::uint8_t>(value >> 8u);
            data[at + 1u] = static_cast<std::uint8_t>(value);
        };
        write(0u, 1u);
        write(4u, 100u);
        write(6u, 10u);
        write(16u, 26u);
        write(36u, 16u);
        write(38u, 1u);
        for (std::size_t corner = 0u; corner < 4u; ++corner)
            write(40u + corner * 2u, 24u);
        write(52u, 1u);
        write(54u, 1u);
        write(56u, glyph);
        write(60u, 0u);
        const auto previous               = Detail::testRichMathTableOverride;
        const auto restore                = wil::scope_exit([&]() noexcept { Detail::testRichMathTableOverride = previous; });
        Detail::testRichMathTableOverride = data;
        const auto expression             = std::make_shared<const MathExpression>(ParseMath(L"1_1^1"));
        const auto prepare                = [&]
        {
            auto result = std::make_unique<Detail::PreparedMath>(factory, expression, 24.f);
            while (! result->Advance())
            {
            }
            return result;
        };
        write(62u, 0u);
        const auto unkerned = prepare();
        write(62u, static_cast<std::uint16_t>(-100));
        const auto kerned = prepare();
        const auto& a     = unkerned->GlyphLayouts();
        const auto& b     = kerned->GlyphLayouts();
        check(a.size() == 3u && b.size() == 3u, "single-glyph math script oracle keeps the base and both scripts");
        const auto expected = -100.f * (b[0].font + b[1].font) / fontMetrics.designUnitsPerEm;
        check(std::abs((b[1].origin.x - a[1].origin.x) - expected) < .05f && std::abs((b[2].origin.x - a[2].origin.x) - expected) < .05f,
              "real SDK-shaped glyphs apply the synthetic OpenType correction to both script positions");
        const auto compoundExpression = std::make_shared<const MathExpression>(ParseMath(L"{xx}_x"));
        const auto prepareCompound    = [&]
        {
            auto result = std::make_unique<Detail::PreparedMath>(factory, compoundExpression, 24.f);
            while (! result->Advance())
            {
            }
            return result;
        };
        const auto compoundKerned = prepareCompound();
        write(62u, 0u);
        const auto compoundPlain = prepareCompound();
        check(compoundKerned->GlyphLayouts().size() == compoundPlain->GlyphLayouts().size() &&
                  std::abs(compoundKerned->GlyphLayouts().back().origin.x - compoundPlain->GlyphLayouts().back().origin.x) < .01f,
              "compound script bases retain the specified zero-kern box fallback");
        write(62u, static_cast<std::uint16_t>(-100));
        write(16u, 0u);
        const auto missing = prepare();
        check(std::abs(missing->GlyphLayouts()[1].origin.x - a[1].origin.x) < .01f && std::abs(missing->GlyphLayouts()[2].origin.x - a[2].origin.x) < .01f,
              "missing MATH kern information falls back to the original placement");
    }
    const auto readsBefore   = Detail::testRichMathConstantReads;
    const auto formatsBefore = Detail::testRichMathTextFormats;
    const auto formats       = std::make_shared<Detail::MathTextFormats>(factory);
    auto cachedExpression    = std::make_shared<const MathExpression>(ParseMath(L"\\left(\\frac{x_i}{y^2}\\right)"));
    Detail::PreparedMath anchor(factory, cachedExpression, 24.f, false, nullptr, 96.f, formats);
    while (! anchor.Advance())
    {
    }
    const auto anchorFormats = Detail::testRichMathTextFormats - formatsBefore;
    check(anchorFormats > 0u && anchorFormats < 16u && formats->Size() == anchorFormats,
          "the independent expression exercises a bounded set of mathematical text styles");
    for (std::size_t i = 1u; i < 128u; ++i)
    {
        Detail::PreparedMath item(factory, cachedExpression, 24.f, false, &anchor, 96.f, formats);
        while (! item.Advance())
        {
        }
        check(item.Size().width > 0.f && item.Size().height > 0.f, "cached font data prepares every admitted object");
    }
    check(Detail::testRichMathConstantReads == readsBefore + 1u, "128 formulas share one generation-scoped font table snapshot");
    check(Detail::testRichMathTextFormats == formatsBefore + anchorFormats,
          "128 formulas reuse one text format per size and style instead of one format per leaf");
    const auto configured = std::make_shared<Detail::MathTextFormats>(factory, L"Segoe UI", L"fr-FR");
    auto* custom          = configured->Get(22.f, true, true);
    std::wstring family(custom->GetFontFamilyNameLength() + 1u, L'\0'), locale(custom->GetLocaleNameLength() + 1u, L'\0');
    THROW_IF_FAILED(custom->GetFontFamilyName(family.data(), static_cast<UINT32>(family.size())));
    THROW_IF_FAILED(custom->GetLocaleName(locale.data(), static_cast<UINT32>(locale.size())));
    family.pop_back();
    locale.pop_back();
    // DirectWrite canonicalizes the supplied fr-FR name to fr-fr. Locale tags
    // are case-insensitive; query the real SDK property, not its input spelling.
    check(family == L"Segoe UI" && CompareStringOrdinal(locale.data(), static_cast<int>(locale.size()), L"fr-FR", 5, TRUE) == CSTR_EQUAL &&
              custom->GetFontWeight() == DWRITE_FONT_WEIGHT_BOLD && custom->GetFontStyle() == DWRITE_FONT_STYLE_ITALIC && custom->GetFontSize() == 22.f &&
              configured->Get(22.f, true, true) == custom,
          "native formats retain explicit mathematical family, locale, size and style with stable reuse");
    Detail::PreparedMath differentFamily(factory, cachedExpression, 24.f, false, &anchor, 96.f, configured);
    while (! differentFamily.Advance())
    {
    }
    check(Detail::testRichMathConstantReads == readsBefore + 2u, "font constants are never borrowed from another mathematical family");
#endif
    for (const auto& [source, count, rising, falling] : std::array{std::tuple{L"\\boxed{x}", 4u, false, false},
                                                                   std::tuple{L"\\cancel{x}", 1u, true, false},
                                                                   std::tuple{L"\\bcancel{x}", 1u, false, true},
                                                                   std::tuple{L"\\xcancel{x}", 2u, true, true},
                                                                   std::tuple{L"\\sout{x}", 1u, false, false},
                                                                   std::tuple{L"\\phase{x}", 2u, true, false},
                                                                   std::tuple{L"\\angl{x}", 2u, false, false}})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(source));
        check(static_cast<bool>(*expression), "bounded annotation parses without losing its child");
        Detail::PreparedMath annotation(factory, expression, 24.f);
        while (! annotation.Advance())
        {
        }
        const auto& strokes = annotation.StrokeLayouts();
        check(strokes.size() == count, "annotation publishes the exact staged stroke count");
        bool hasRising = false, hasFalling = false;
        for (const auto& line : strokes)
        {
            hasRising |= line.first.x < line.last.x && line.first.y > line.last.y;
            hasFalling |= line.first.x < line.last.x && line.first.y < line.last.y;
            const auto size = annotation.Size();
            check(line.first.x >= 0.f && line.last.x >= 0.f && line.first.x <= size.width && line.last.x <= size.width && line.first.y >= 0.f &&
                      line.last.y >= 0.f && line.first.y <= size.height && line.last.y <= size.height,
                  "annotation strokes remain in the published extent");
        }
        check(hasRising == rising && hasFalling == falling, "annotation orientation matches the authored command");
        if (std::wstring_view(source) == L"\\phase{x}" || std::wstring_view(source) == L"\\angl{x}")
        {
            const auto& first  = strokes[0];
            const auto& second = strokes[1];
            if (std::wstring_view(source) == L"\\phase{x}")
                check(first.first.y > first.last.y && first.first.x == second.first.x && first.first.y == second.first.y && second.first.y == second.last.y,
                      "phase has the independently specified rising left leg and bottom bar");
            else
                check(first.first.y == first.last.y && first.last.x == second.first.x && first.last.y == second.first.y && second.first.x == second.last.x &&
                          second.first.y < second.last.y,
                      "actuarial angle has a top bar and right leg");
        }
    }
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto& [name, strokes] : std::array{std::pair{L"check", 2u},
                                                  std::pair{L"widecheck", 2u},
                                                  std::pair{L"breve", 4u},
                                                  std::pair{L"acute", 1u},
                                                  std::pair{L"grave", 1u},
                                                  std::pair{L"mathring", 8u},
                                                  std::pair{L"underbar", 1u},
                                                  std::pair{L"utilde", 24u}})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\" + std::wstring(name) + L"{x}"));
        check(static_cast<bool>(*expression), "bounded accent has a complete operand");
        Detail::PreparedMath layout(factory, expression, 24.f);
        while (! layout.Advance())
        {
        }
        const auto lines = layout.StrokeLayouts();
        check(lines.size() == strokes, "each accent stages its own bounded geometry");
        if (std::wstring_view(name) == L"check" || std::wstring_view(name) == L"widecheck")
            check(lines[0u].first.y < lines[0u].last.y && lines[1u].first.y > lines[1u].last.y, "check accents open upward");
        if (std::wstring_view(name) == L"acute")
            check(lines[0u].first.y > lines[0u].last.y, "acute slopes upward");
        if (std::wstring_view(name) == L"grave")
            check(lines[0u].first.y < lines[0u].last.y, "grave slopes downward");
        if (std::wstring_view(name) == L"underbar" || std::wstring_view(name) == L"utilde")
            check(lines[0u].first.y > layout.Baseline(), "under accents remain below the body baseline");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto& [name, script, above] : std::array{std::tuple{L"overbrace", L"^{n}", true},
                                                        std::tuple{L"underbrace", L"_{n}", false},
                                                        std::tuple{L"overgroup", L"^{n}", true},
                                                        std::tuple{L"undergroup", L"_{n}", false}})
    {
        auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\" + std::wstring(name) + L"{x}" + script));
        check(static_cast<bool>(*expression), "bounded brace or group with annotation parses");
        Detail::PreparedMath layout(factory, expression, 24.f);
        while (! layout.Advance())
        {
        }
        const auto glyphs = layout.GlyphLayouts();
        const auto lines  = layout.StrokeLayouts();
        check(glyphs.size() == 2u && lines.size() == 6u && (above ? glyphs[1u].origin.y < glyphs[0u].origin.y : glyphs[1u].origin.y > glyphs[0u].origin.y),
              "brace or group stages a distinct outline and places its annotation above or below the body");
        const auto center = lines[2u].last.y;
        check(above ? center < lines[0u].first.y : center > lines[0u].first.y, "brace or group curvature faces away from its body");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto& [name, strokes] : std::array{std::pair{L"xrightarrow", 3u},
                                                  std::pair{L"xleftarrow", 3u},
                                                  std::pair{L"xleftrightarrow", 5u},
                                                  std::pair{L"xRightarrow", 4u},
                                                  std::pair{L"xLeftarrow", 4u},
                                                  std::pair{L"xLeftrightarrow", 6u},
                                                  std::pair{L"xmapsto", 4u},
                                                  std::pair{L"xlongequal", 2u}})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\" + std::wstring(name) + L"[below]{above}"));
        check(static_cast<bool>(*expression), "extensible relation with optional lower and required upper text parses");
        Detail::PreparedMath layout(factory, expression, 24.f);
        while (! layout.Advance())
        {
        }
        const auto glyphs = layout.GlyphLayouts();
        check(glyphs.size() == 2u && layout.StrokeLayouts().size() == strokes && glyphs[0u].origin.y < glyphs[1u].origin.y,
              "extensible relation has its specified heads and places upper and lower labels separately");
        check(layout.Size().width >= glyphs[0u].extent.width && layout.Size().width >= glyphs[1u].extent.width, "arrow shaft fits its widest label");
    }
#endif
    check(! ParseMath(L"\\xrightarrow[unfinished{x}") && ! ParseMath(L"\\xrightarrow"), "unclosed or missing arrow labels reject the complete formula");
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto source : {L"\\mathbf{\\textmd{x}}", L"\\mathit{\\textup{x}}", L"\\mathbf{\\textnormal{x}}"})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(source));
        check(static_cast<bool>(*expression), "bounded text-style reset parses");
        Detail::PreparedMath layout(factory, expression, 24.f);
        while (! layout.Advance())
        {
        }
        check(layout.GlyphLayouts().size() == 1u && ! layout.GlyphLayouts()[0u].bold && ! layout.GlyphLayouts()[0u].italic,
              "text medium, upright and normal commands cancel the enclosing corresponding style");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        auto accentFormats = std::make_shared<Detail::MathTextFormats>(factory, L"Arial");
        for (const auto name : {L"hat", L"widehat"})
        {
            const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\" + std::wstring(name) + L"{x}"));
            Detail::PreparedMath layout(factory, expression, 24.f, false, nullptr, 96.f, accentFormats);
            while (! layout.Advance())
            {
            }
            const auto lines = layout.StrokeLayouts();
            check(layout.VariantLayouts().empty() && lines.size() == 2u && lines[0u].first.y > lines[0u].last.y && lines[1u].first.y < lines[1u].last.y &&
                      lines[0u].last.x == lines[1u].first.x,
                  "non-MATH font uses the independently verified triangular hat fallback");
        }
        for (const auto name : {L"hat",
                                L"widehat",
                                L"check",
                                L"widecheck",
                                L"breve",
                                L"acute",
                                L"grave",
                                L"mathring",
                                L"underbar",
                                L"utilde",
                                L"overbrace",
                                L"underbrace",
                                L"overgroup",
                                L"undergroup"})
            for (const float size : {1.f, 2.f, 24.f})
                for (const float dpi : {96.f, 144.f, 192.f})
                {
                    const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\" + std::wstring(name) + L"{x}"));
                    Detail::PreparedMath layout(factory, expression, size, false, nullptr, dpi, formats);
                    while (! layout.Advance())
                    {
                    }
                    const auto bounds = layout.Size();
                    for (const auto& line : layout.StrokeLayouts())
                    {
                        const float half = line.thickness * .5f;
                        check(std::min(line.first.x, line.last.x) - half >= -.001f && std::max(line.first.x, line.last.x) + half <= bounds.width + .001f &&
                                  std::min(line.first.y, line.last.y) - half >= -.001f && std::max(line.first.y, line.last.y) + half <= bounds.height + .001f,
                              "small and scaled accent strokes remain inside final prepared ink bounds");
                    }
                }
    }
#endif
    check(static_cast<bool>(ParseMath(L"\\boxed{\\xcancel{a+b}}")) && ! ParseMath(L"\\boxed") && ! ParseMath(L"\\cancel{"),
          "nested annotations parse and incomplete operands fail closed");
    // The named sizes and factors are pinned independently to KaTeX 0.16.11 Options.js.
    for (const auto& [name, factor] : std::array{std::pair{L"tiny", .5f},
                                                 std::pair{L"sixptsize", .6f},
                                                 std::pair{L"scriptsize", .7f},
                                                 std::pair{L"footnotesize", .8f},
                                                 std::pair{L"small", .9f},
                                                 std::pair{L"normalsize", 1.f},
                                                 std::pair{L"large", 1.2f},
                                                 std::pair{L"Large", 1.44f},
                                                 std::pair{L"LARGE", 1.728f},
                                                 std::pair{L"huge", 2.074f},
                                                 std::pair{L"Huge", 2.488f}})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\" + std::wstring(name) + L" x"));
        check(static_cast<bool>(*expression), "named size declaration parses");
        Detail::PreparedMath sized(factory, expression, 24.f);
        while (! sized.Advance())
        {
        }
        check(sized.GlyphLayouts().size() == 1u && std::abs(sized.GlyphLayouts()[0].font - 24.f * factor) < .001f, "named size shapes at the pinned factor");
    }
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(L"{\\Huge x_y}z"));
        check(static_cast<bool>(*expression), "named size remains scoped across a grouped script");
        Detail::PreparedMath sized(factory, expression, 24.f);
        while (! sized.Advance())
        {
        }
        const auto& glyphs = sized.GlyphLayouts();
        check(glyphs.size() == 3u && std::abs(glyphs[0].font - 24.f * 2.488f) < .001f && std::abs(glyphs[1].font - 24.f * 2.074f) < .001f &&
                  std::abs(glyphs[2].font - 24.f) < .001f,
              "named size uses the pinned script map and ends at the group boundary");
    }
    for (const auto source : {L"x+1\\tag{A}", L"\\tag*{2}x+1"})
    {
        const auto tagged = ParseMath(source);
        check(static_cast<bool>(tagged) && tagged.root.kind == MathKind::Style && tagged.root.text == L"tag" && tagged.root.children.size() == 2u,
              "one root tag retains independent body and label syntax");
        bool rejectedDirect = false;
        try
        {
            Detail::PreparedMath direct(factory, std::make_shared<const MathExpression>(tagged), 24.f, false);
        }
        catch (const std::length_error&)
        {
            rejectedDirect = true;
        }
        check(rejectedDirect, "tagged root cannot be silently prepared as an inline formula");
    }
    check(! ParseMath(L"x\\tag{a}\\tag{b}") && ! ParseMath(L"{x\\tag{a}}") && ! ParseMath(L"x\\tag"), "duplicate, nested and missing tags are rejected");
    const auto ordinaryTag = ParseMath(L"x\\tag{Eq. 1}");
    const auto starredTag  = ParseMath(L"x\\tag*{A}");
    const auto escapedTag  = ParseMath(L"x\\tag*{\\$5}");
    check(! ParseMath(L"x\\tag{$y$}") && ordinaryTag && ordinaryTag.root.children.size() == 2u && ordinaryTag.root.children[1].children.size() == 3u &&
              ordinaryTag.root.children[1].children[1].text == L"Eq. 1" && starredTag && starredTag.root.children.size() == 2u &&
              starredTag.root.children[1].text == L"A",
          "tag labels preserve text spaces and starred form while rejecting unsupported embedded math");
    check(escapedTag && escapedTag.root.children.size() == 2u && escapedTag.root.children[1].text == L"$5",
          "escaped tag dollars remain literal text without admitting raw math delimiters");
    std::size_t scenarioIndex{};
    for (const auto& scenario : prepared)
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(scenario.source));
        check(static_cast<bool>(*expression), "every admitted independent native math fixture parses completely");
        if (scenarioIndex < std::size(targetShapes))
        {
            const auto actual = Shape(expression->root);
            if (actual != targetShapes[scenarioIndex])
                std::wcerr << scenario.id << L" expected " << targetShapes[scenarioIndex] << L" actual " << actual << L'\n';
            check(actual == targetShapes[scenarioIndex], "named target expression retains its exact ordered AST");
        }
        ++scenarioIndex;
        for (const bool display : {false, true})
        {
            Detail::PreparedMath layout(factory, expression, 24.f, display);
            unsigned turns{};
            while (! layout.Advance())
                check(++turns <= 512u, "each formula has a finite bounded leaf preparation");
            const auto size = layout.Size();
            check(std::isfinite(size.width + size.height) && size.width >= 1.f && size.height >= 1.f && layout.Advance() && layout.Size().width == size.width &&
                      layout.Size().height == size.height,
                  "both native math styles publish finite stable geometry and completed preparation is idempotent");
        }
    }
    for (const auto& scenario : rejected)
        check(! ParseMath(scenario.source), "malformed or excluded native math never publishes a partial expression");
    // Named target diagnostics are independent of the user-facing fallback text.
    const std::pair<std::wstring_view, MathIssue> issues[] = {
        {L"\\frac{a}", MathIssue::MissingRequiredArgument},
        {L"\\sqrt[3{x}", MathIssue::MissingRootIndexClose},
        {L"x_", MathIssue::MissingScriptArgument},
        {L"x_1_2", MathIssue::DuplicateSubscript},
        {L"{x", MathIssue::MissingGroupClose},
        {L"\\left(x", MathIssue::MissingRightDelimiter},
        {L"\\begin{matrix}a\\end{cases}", MathIssue::MismatchedEnvironment},
        {L"a&b", MathIssue::CellSeparatorOutsideEnvironment},
        {L"\\newcommand{\\foo}{x}", MathIssue::UnsupportedCommand},
        {L"\\input{local}", MathIssue::UnsupportedCommand},
        {L"\\unknown{x}", MathIssue::UnsupportedCommand},
        {L"\\begin{CD}a&b\\end{CD}", MathIssue::UnsupportedEnvironment},
    };
    for (const auto& [source, issue] : issues)
    {
        auto result = ParseMath(source);
        check(! result && result.issue == issue, "named target failure has its exact typed diagnostic");
        auto moved = std::move(result);
        check(moved.issue == issue && ! moved.diagnostic.empty(), "math diagnostic survives moving its result");
    }
    for (const auto& symbol : symbols)
    {
        const auto expression = ParseMath(L"\\" + std::wstring(symbol.command));
        check(expression && expression.root.children.size() == 1u && expression.root.children[0u].kind == MathKind::Text &&
                  expression.root.children[0u].text == symbol.value,
              "named mathematical symbols retain independently specified Unicode identity");
    }
    for (const auto name : {L"boldsymbol", L"bm", L"mathnormal"})
    {
        const auto expression = ParseMath(L"\\" + std::wstring(name) + L"{x+1}");
        check(expression && expression.root.children.size() == 1u && expression.root.children[0u].kind == MathKind::Style &&
                  expression.root.children[0u].text == name,
              "bounded math font commands retain distinct scoped style nodes");
    }
    const auto operators = ParseMath(L"a-b*c~d");
    check(operators && operators.root.children.size() == 7u && operators.root.children[1u].text == L"−" &&
              operators.root.children[1u].atomClass == MathAtomClass::Binary && operators.root.children[3u].text == L"∗" &&
              operators.root.children[3u].atomClass == MathAtomClass::Binary && operators.root.children[5u].kind == MathKind::Space,
          "math-mode ASCII operators use their mathematical glyphs and tilde is spacing");
    for (const auto& [source, glyph] : {std::pair{L"\\not=", L"≠"},
                                        std::pair{L"\\not<", L"≮"},
                                        std::pair{L"\\not>", L"≯"},
                                        std::pair{L"\\not\\in", L"∉"},
                                        std::pair{L"\\not\\ni", L"∌"},
                                        std::pair{L"\\not\\leq", L"≰"},
                                        std::pair{L"\\not\\geq", L"≱"},
                                        std::pair{L"\\not\\subset", L"⊄"},
                                        std::pair{L"\\not\\supset", L"⊅"},
                                        std::pair{L"\\not\\subseteq", L"⊈"},
                                        std::pair{L"\\not\\supseteq", L"⊉"},
                                        std::pair{L"\\not\\sim", L"≁"},
                                        std::pair{L"\\not\\equiv", L"≢"},
                                        std::pair{L"\\not\\approx", L"≉"},
                                        std::pair{L"\\not\\mid", L"∤"},
                                        std::pair{L"\\not\\parallel", L"∦"}})
    {
        const auto negated = ParseMath(source);
        check(negated && negated.root.children.size() == 1u && negated.root.children[0u].text == glyph &&
                  negated.root.children[0u].atomClass == MathAtomClass::Relation,
              "supported negation retains its exact Unicode relation glyph and spacing class");
    }
    check(! ParseMath(L"\\not x") && ! ParseMath(L"\\not\\frac{1}{2}"),
          "unsupported negation operands retain source instead of drawing an approximate overlay");
    const auto prime = ParseMath(L"f''_i");
    check(prime && prime.root.children.size() == 1u && prime.root.children[0u].kind == MathKind::Scripts && prime.root.children[0u].children[1u].text == L"i" &&
              prime.root.children[0u].children[2u].text == L"′′",
          "prime runs are superscripts on the preceding atom and coexist with subscripts");
    check(! ParseMath(L"f'^2"), "a prime followed by a second superscript is rejected rather than misrendered");
    const auto norm = ParseMath(L"\\|x\\|");
    check(norm && norm.root.children.size() == 3u && norm.root.children[0u].text == L"‖" && norm.root.children[2u].text == L"‖",
          "escaped double vertical bars render independently as norm atoms");
    const auto scalableNorm = ParseMath(L"\\left\\|x\\right\\|");
    check(scalableNorm && scalableNorm.root.children.size() == 1u && scalableNorm.root.children[0u].kind == MathKind::Delimited &&
              scalableNorm.root.children[0u].text == L"‖‖",
          "both scalable norm delimiters retain their double-bar glyph identity");
    const auto sizedNorm = ParseMath(L"\\big\\|");
    check(sizedNorm && sizedNorm.root.children.size() == 1u && sizedNorm.root.children[0u].kind == MathKind::Delimiter &&
              sizedNorm.root.children[0u].text == L"‖" && sizedNorm.root.children[0u].delimiterSize == 1u,
          "sized norm delimiter preserves its double-bar glyph and requested size");
    for (const auto& [source, glyphs] : {std::pair{L"\\left\\lparen x\\right\\rparen", L"()"},
                                         std::pair{L"\\left\\lt x\\right\\gt", L"<>"},
                                         std::pair{L"\\left/ x\\right\\backslash", L"/\\"}})
    {
        const auto delimited = ParseMath(source);
        check(delimited && delimited.root.children.size() == 1u && delimited.root.children[0u].kind == MathKind::Delimited &&
                  delimited.root.children[0u].text == glyphs,
              "parenthesis aliases, relation-shaped and slash delimiters retain exact glyph identity");
    }
    for (const auto& [source, kind, label] : std::array{std::tuple{L"a\\over b", MathKind::Fraction, L"frac"},
                                                        std::tuple{L"a\\atop b", MathKind::Fraction, L"atop"},
                                                        std::tuple{L"a\\choose b", MathKind::Matrix, L"pmatrix"},
                                                        std::tuple{L"a\\brace b", MathKind::Matrix, L"Bmatrix"},
                                                        std::tuple{L"a\\brack b", MathKind::Matrix, L"bmatrix"}})
    {
        const auto expression = ParseMath(source);
        check(expression && expression.root.kind == kind && expression.root.text == label && expression.root.children.size() == 2u &&
                  expression.root.children[0u].children[0u].text == L"a" && expression.root.children[1u].children[0u].text == L"b",
              "infix math owns both sides and preserves its independently specified rule and fences");
    }
    check(! ParseMath(L"a\\over b\\atop c") && ! ParseMath(L"a\\over \\displaystyle b\\over c"), "a second infix in the same math group is rejected");
    check(static_cast<bool>(ParseMath(L"{a\\over {b\\atop c}}")), "nested groups each admit their own infix");
#if DXUI_ENABLE_DIAGNOSTICS
    ULONG_PTR stackLow{}, stackHigh{};
    GetCurrentThreadStackLimits(&stackLow, &stackHigh);
    Detail::testRichMathLowestStackAddress = (std::numeric_limits<std::uintptr_t>::max)();
#endif
    std::wstring nestedInfix(0u, L' ');
    for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
        nestedInfix += L"{a\\over ";
    nestedInfix += L'b';
    nestedInfix.append(MathMaximumGroupDepth, L'}');
    check(static_cast<bool>(ParseMath(nestedInfix)), "nested infix fractions honor the 64-group bound on the ordinary stack");
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto& [source, expectedStrokes] : {std::pair{L"a\\over b", 1u}, std::pair{L"a\\atop b", 0u}})
    {
        Detail::PreparedMath fractionLayout(factory, std::make_shared<const MathExpression>(ParseMath(source)), 24.f);
        while (! fractionLayout.Advance())
        {
        }
        check(fractionLayout.StrokeLayouts().size() == expectedStrokes && fractionLayout.GlyphLayouts().size() == 2u,
              "fraction layout paints a rule only for over while preserving both SDK-shaped operands");
    }
#endif
    {
        const auto expression = ParseMath(L"\\begin{array}{lcr}a&aa&aaa\\\\aa&a&a\\end{array}");
        check(expression && expression.root.children.size() == 1u && expression.root.children[0u].text == L"array:lcr" &&
                  expression.root.children[0u].columns == 3u && expression.root.children[0u].children.size() == 6u,
              "array retains its explicit ordered alignment and rectangular cells");
#if DXUI_ENABLE_DIAGNOSTICS
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        const auto glyphs = layout.GlyphLayouts();
        check(glyphs.size() == 6u && std::abs(glyphs[0u].origin.x - glyphs[3u].origin.x) < .01f &&
                  std::abs(glyphs[1u].origin.x + glyphs[1u].extent.width * .5f - glyphs[4u].origin.x - glyphs[4u].extent.width * .5f) < .01f &&
                  std::abs(glyphs[2u].origin.x + glyphs[2u].extent.width - glyphs[5u].origin.x - glyphs[5u].extent.width) < .01f,
              "SDK-shaped cells share their independently specified left, center and right anchors");
#endif
    }
    for (const auto source :
         {L"\\begin{array}{}a\\end{array}", L"\\begin{array}{l!r}a&b\\end{array}", L"\\begin{array}{l}a&b\\end{array}", L"\\begin{array}{z}a\\end{array}"})
        check(! ParseMath(source), "unsupported or overwide array specifications retain source instead of partial rendering");
    {
        const auto expression = ParseMath(L"\\genfrac{(}{)}{0pt}{0}{a}{b}");
        check(expression && expression.root.children.size() == 1u && expression.root.children[0u].kind == MathKind::Style &&
                  expression.root.children[0u].text == L"displaystyle" && expression.root.children[0u].children[0u].kind == MathKind::Delimited &&
                  expression.root.children[0u].children[0u].text == L"()" && expression.root.children[0u].children[0u].children[0u].text == L"rule:0pt",
              "general fraction records independently specified style, fences and absent rule");
#if DXUI_ENABLE_DIAGNOSTICS
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        check(layout.GlyphLayouts().size() == 2u && layout.Size().width > 0.f, "general fraction prepares both SDK-shaped operands and fences");
#endif
    }
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto& [source, expectedLines] : {std::pair{L"\\xhookleftarrow{a}", 4u},
                                                std::pair{L"\\xhookrightarrow{a}", 4u},
                                                std::pair{L"\\xleftharpoonup{a}", 2u},
                                                std::pair{L"\\xleftharpoondown{a}", 2u},
                                                std::pair{L"\\xrightharpoonup{a}", 2u},
                                                std::pair{L"\\xrightharpoondown{a}", 2u},
                                                std::pair{L"\\xleftrightharpoons{a}", 4u},
                                                std::pair{L"\\xrightleftharpoons{a}", 4u},
                                                std::pair{L"\\xtwoheadleftarrow{a}", 5u},
                                                std::pair{L"\\xtwoheadrightarrow{a}", 5u},
                                                std::pair{L"\\xtofrom{a}", 6u}})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(source));
        check(static_cast<bool>(*expression), "hook and harpoon relations parse");
        Detail::PreparedMath arrow(factory, expression, 24.f);
        while (! arrow.Advance())
        {
        }
        check(arrow.StrokeLayouts().size() == expectedLines && arrow.GlyphLayouts().size() == 1u,
              "hook and harpoon relations retain their bounded directed strokes and label");
        const auto& strokes = arrow.StrokeLayouts();
        for (const auto& line : strokes)
            check(line.first.x >= 0.f && line.last.x >= 0.f && line.first.x <= arrow.Size().width && line.last.x <= arrow.Size().width && line.first.y >= 0.f &&
                      line.last.y >= 0.f && line.first.y <= arrow.Size().height && line.last.y <= arrow.Size().height,
                  "directed arrow stroke endpoints remain in the prepared box");
        const std::wstring_view command(source);
        if (expectedLines == 2u && strokes.size() == 2u)
        {
            const auto& head = strokes[1u];
            const bool left  = command.find(L"leftharpoon") != command.npos;
            const bool up    = command.find(L"up") != command.npos;
            check((left ? head.first.x > head.last.x : head.first.x < head.last.x) && (up ? head.first.y < head.last.y : head.first.y > head.last.y),
                  "single harpoon endpoint independently identifies direction and barb orientation");
        }
        if (expectedLines == 4u && strokes.size() == 4u && command.find(L"harpoons") != command.npos)
        {
            const bool leftFirst = command.find(L"leftright") != command.npos;
            check(strokes[0u].first.y < strokes[1u].first.y &&
                      (leftFirst ? strokes[2u].first.x > strokes[2u].last.x : strokes[2u].first.x < strokes[2u].last.x) &&
                      (leftFirst ? strokes[3u].first.x < strokes[3u].last.x : strokes[3u].first.x > strokes[3u].last.x),
                  "paired harpoons use two ordered shafts with opposing heads");
        }
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto& [name, count, below] : {std::tuple{L"overleftarrow", 3u, false},
                                             std::tuple{L"overrightarrow", 3u, false},
                                             std::tuple{L"overleftrightarrow", 5u, false},
                                             std::tuple{L"Overrightarrow", 4u, false},
                                             std::tuple{L"underleftarrow", 3u, true},
                                             std::tuple{L"underrightarrow", 3u, true},
                                             std::tuple{L"underleftrightarrow", 5u, true},
                                             std::tuple{L"overleftharpoon", 2u, false},
                                             std::tuple{L"overrightharpoon", 2u, false}})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\" + std::wstring(name) + L"{abc}"));
        check(static_cast<bool>(*expression), "arrow accent parses completely");
        Detail::PreparedMath accent(factory, expression, 24.f);
        while (! accent.Advance())
        {
        }
        const auto& strokes            = accent.StrokeLayouts();
        const auto& glyphs             = accent.GlyphLayouts();
        const auto [inkTop, inkBottom] = glyphs.size() == 1u ? GlyphInkBounds(glyphs[0u], check) : std::pair{0.f, 0.f};
        check(strokes.size() == count && glyphs.size() == 1u && (below ? strokes[0u].first.y > inkBottom : strokes[0u].first.y < inkTop),
              "directed accent keeps its distinct head and above/below placement");
        if (strokes.size() >= 2u)
        {
            const std::wstring_view accentName(name);
            const bool left = accentName.find(L"left") != accentName.npos;
            check(left ? strokes[1u].first.x > strokes[1u].last.x : strokes[1u].first.x < strokes[1u].last.x, "accent head points in the authored direction");
        }
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto name : {L"overlinesegment", L"underlinesegment"})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\" + std::wstring(name) + L"{abc}"));
        check(static_cast<bool>(*expression), "line-segment accent parses completely");
        Detail::PreparedMath accent(factory, expression, 24.f);
        while (! accent.Advance())
        {
        }
        const auto& lines              = accent.StrokeLayouts();
        const auto& glyphs             = accent.GlyphLayouts();
        const auto [inkTop, inkBottom] = glyphs.size() == 1u ? GlyphInkBounds(glyphs[0u], check) : std::pair{0.f, 0.f};
        check(lines.size() == 3u && lines[0u].first.y == lines[0u].last.y && lines[1u].first.x == lines[1u].last.x && lines[2u].first.x == lines[2u].last.x &&
                  lines[1u].first.x < lines[2u].first.x && glyphs.size() == 1u &&
                  ((std::wstring_view(name).starts_with(L"under")) ? lines[0u].first.y > inkBottom : lines[0u].first.y < inkTop),
              "line-segment accents stage a horizontal shaft with distinct bounded end bars");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto& [name, count] : {std::pair{L"dot", 1u}, std::pair{L"ddot", 2u}})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\" + std::wstring(name) + L"{x}"));
        Detail::PreparedMath accent(factory, expression, 24.f);
        while (! accent.Advance())
        {
        }
        const auto& marks = accent.StrokeLayouts();
        check(marks.size() == count && std::ranges::all_of(marks,
                                                           [](const auto& mark)
        { return mark.dot && mark.thickness > 0.f && mark.first.x == mark.last.x && mark.first.y == mark.last.y; }),
              "dot accents stage filled circular marks instead of flat line strokes");
        if (marks.size() == 2u)
            check(marks[0u].first.x < marks[1u].first.x && marks[0u].first.y == marks[1u].first.y, "double dots stage two horizontally ordered circles");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\textsf{\\textbf{H}}"));
        check(static_cast<bool>(*expression), "text family and weight commands stack");
        check(static_cast<bool>(ParseMath(L"\\text{a$b}")), "ordinary text retains its existing literal dollar policy");
        const auto groupedExpression = std::make_shared<const MathExpression>(ParseMath(L"\\text{a { b}}"));
        const auto flatExpression    = std::make_shared<const MathExpression>(ParseMath(L"\\text{a b}"));
        Detail::PreparedMath grouped(factory, groupedExpression, 24.f), flat(factory, flatExpression, 24.f);
        while (! grouped.Advance())
        {
        }
        while (! flat.Advance())
        {
        }
        check(std::abs(grouped.Size().width - flat.Size().width) < .01f, "plain grouped text collapses whitespace across group boundaries");
        const auto colorExpression = std::make_shared<const MathExpression>(ParseMath(L"\\text{a \\textcolor{red}{ b}}"));
        const auto colorDollar     = ParseMath(L"\\text{a \\textcolor{red}{$b}}");
        check(static_cast<bool>(*colorExpression) && static_cast<bool>(colorDollar), "nested transparent color retains text policy");
        Detail::PreparedMath colorGrouped(factory, colorExpression, 24.f);
        while (! colorGrouped.Advance())
        {
        }
        check(std::abs(colorGrouped.Size().width - flat.Size().width) < .01f, "transparent text-color group preserves surrounding whitespace collapse");
        auto textFormats = std::make_shared<Detail::MathTextFormats>(factory, L"Cambria Math", L"en-us", L"Arial", L"Consolas");
        Detail::PreparedMath layout(factory, expression, 24.f, false, nullptr, 96.f, textFormats);
        while (! layout.Advance())
        {
        }
        const auto& glyphs = layout.GlyphLayouts();
        check(glyphs.size() == 1u, "nested text style retains one shaped leaf");
        if (glyphs.size() == 1u)
        {
            DWRITE_FONT_WEIGHT weight{};
            THROW_IF_FAILED(glyphs[0u].layout->GetFontWeight(0u, &weight));
            UINT32 length{};
            THROW_IF_FAILED(glyphs[0u].layout->GetFontFamilyNameLength(0u, &length));
            std::wstring familyName(length + 1u, L'\0');
            THROW_IF_FAILED(glyphs[0u].layout->GetFontFamilyName(0u, familyName.data(), static_cast<UINT32>(familyName.size())));
            familyName.resize(length);
            check(weight == DWRITE_FONT_WEIGHT_BOLD && familyName == L"Arial", "DirectWrite retains stacked text weight and family");
        }
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto plain = std::make_shared<const MathExpression>(ParseMath(L"a+b"));
        for (const auto source : {L"a\\notag+b", L"a\\nonumber+b"})
        {
            const auto expression = std::make_shared<const MathExpression>(ParseMath(source));
            check(static_cast<bool>(*expression), "equation-number suppression parses as a declaration");
            Detail::PreparedMath original(factory, plain, 24.f), suppressed(factory, expression, 24.f);
            while (! original.Advance())
            {
            }
            while (! suppressed.Advance())
            {
            }
            check(std::abs(original.Size().width - suppressed.Size().width) < .01f && original.GlyphLayouts().size() == suppressed.GlyphLayouts().size(),
                  "number suppression adds no painted atom or spacing");
        }
        check(static_cast<bool>(ParseMath(L"\\begin{align}a&=b\\notag\\\\c&=d\\nonumber\\end{align}")),
              "number suppression is accepted independently for aligned rows");
        check(! ParseMath(L"a\\notagged b"), "number-suppression name does not consume a longer unknown command");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\mathscr{AB}"));
        check(static_cast<bool>(*expression), "mathematical script admits bounded uppercase alphabet");
        Detail::PreparedMath script(factory, expression, 24.f);
        while (! script.Advance())
        {
        }
        const auto& glyphs = script.GlyphLayouts();
        check(! glyphs.empty(), "script alphabet stages native glyphs");
        if (! glyphs.empty())
        {
            UINT32 length{};
            THROW_IF_FAILED(glyphs[0u].layout->GetFontFamilyNameLength(0u, &length));
            std::wstring actual(length + 1u, L'\0');
            THROW_IF_FAILED(glyphs[0u].layout->GetFontFamilyName(0u, actual.data(), static_cast<UINT32>(actual.size())));
            actual.resize(length);
            check(actual == L"Segoe Script", "script alphabet uses its distinct verified native face");
        }
        auto missingFormats =
            std::make_shared<Detail::MathTextFormats>(factory, L"Cambria Math", L"en-us", L"Arial", L"Consolas", L"DxUi Missing Script Face 72A13");
        bool missingRefused{};
        try
        {
            Detail::PreparedMath missing(factory, expression, 24.f, false, nullptr, 96.f, missingFormats);
            while (! missing.Advance())
            {
            }
        }
        catch (const wil::ResultException& failure)
        {
            missingRefused = failure.GetErrorCode() == DWRITE_E_UNSUPPORTEDOPERATION;
        }
        check(missingRefused, "missing script face uses local unsupported-source fallback classification");
        Detail::PreparedMath recovered(factory, expression, 24.f);
        while (! recovered.Advance())
        {
        }
        check(! recovered.GlyphLayouts().empty(), "new format generation retries after script face becomes available");
        check(! ParseMath(L"\\mathscr{a}") && ! ParseMath(L"\\mathscr{1}") && ! ParseMath(L"\\mathscr{A+B}"),
              "unsupported script operands retain exact source");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\hat{f}"));
        Detail::PreparedMath accent(factory, expression, 24.f);
        while (! accent.Advance())
        {
        }
        const auto& glyphs  = accent.GlyphLayouts();
        const auto& strokes = accent.StrokeLayouts();
        check(glyphs.size() == 1u && strokes.size() == 2u && glyphs[0u].italic, "italic accent skew fixture retains its shaped base and triangular fallback");
        if (glyphs.size() == 1u && strokes.size() == 2u)
        {
            const float actualCenter   = (strokes[0u].first.x + strokes[1u].last.x) * .5f;
            const float expectedCenter = glyphs[0u].origin.x + glyphs[0u].extent.width * .5f + std::clamp(glyphs[0u].italicCorrection * .5f, 0.f, 6.f);
            check(std::abs(actualCenter - expectedCenter) < .01f, "single italic glyph accent uses bounded shaped-font italic correction");
        }
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        wil::com_ptr<IDWriteFontCollection> skewCollection;
        THROW_IF_FAILED(factory->GetSystemFontCollection(skewCollection.put()));
        UINT32 skewIndex{};
        BOOL exists{};
        THROW_IF_FAILED(skewCollection->FindFamilyName(L"Cambria Math", &skewIndex, &exists));
        check(exists != FALSE, "accent correction oracle has the system math face");
        wil::com_ptr<IDWriteFontFamily> skewFamily;
        wil::com_ptr<IDWriteFont> skewFont;
        wil::com_ptr<IDWriteFontFace> skewFace;
        THROW_IF_FAILED(skewCollection->GetFontFamily(skewIndex, skewFamily.put()));
        THROW_IF_FAILED(skewFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, skewFont.put()));
        THROW_IF_FAILED(skewFont->CreateFontFace(skewFace.put()));
        const UINT32 scalar = L'f';
        UINT16 skewGlyph{};
        DWRITE_FONT_METRICS skewMetrics{};
        THROW_IF_FAILED(skewFace->GetGlyphIndices(&scalar, 1u, &skewGlyph));
        skewFace->GetMetrics(&skewMetrics);
        std::array<std::uint8_t, 64u> skewTable{};
        const auto put = [&](std::size_t offset, std::uint16_t value)
        {
            skewTable[offset]      = static_cast<std::uint8_t>(value >> 8u);
            skewTable[offset + 1u] = static_cast<std::uint8_t>(value);
        };
        put(0u, 1u);
        put(4u, 40u);
        put(6u, 10u);
        put(10u, 8u);
        put(18u, 8u);
        put(20u, 1u);
        put(22u, 300u);
        put(26u, 1u);
        put(28u, 1u);
        put(30u, skewGlyph);
        const auto previousTable          = Detail::testRichMathTableOverride;
        const auto restoreTable           = wil::scope_exit([&]() noexcept { Detail::testRichMathTableOverride = previousTable; });
        Detail::testRichMathTableOverride = skewTable;
        const auto skewExpression         = std::make_shared<const MathExpression>(ParseMath(L"\\hat{f}"));
        Detail::PreparedMath skewAccent(factory, skewExpression, 24.f);
        while (! skewAccent.Advance())
        {
        }
        const auto& skewGlyphs  = skewAccent.GlyphLayouts();
        const auto& skewStrokes = skewAccent.StrokeLayouts();
        check(skewGlyphs.size() == 1u && skewStrokes.size() == 2u && skewGlyph != 0u && skewMetrics.designUnitsPerEm != 0u,
              "SDK accent oracle resolves the real face and glyph");
        if (skewGlyphs.size() == 1u && skewStrokes.size() == 2u && skewMetrics.designUnitsPerEm != 0u)
        {
            const float skewCorrection = 300.f * 24.f / skewMetrics.designUnitsPerEm;
            const float actualCenter   = (skewStrokes[0u].first.x + skewStrokes[1u].last.x) * .5f;
            check(std::abs(skewGlyphs[0u].italicCorrection - skewCorrection) < .01f &&
                      std::abs(actualCenter - skewGlyphs[0u].origin.x - skewGlyphs[0u].extent.width * .5f - skewCorrection * .5f) < .01f,
                  "nonzero MATH correction positions the accent independently of right ink overhang");
        }
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto taggedRows = std::make_shared<const MathExpression>(ParseMath(L"\\begin{align}a&=b\\tag{one}\\\\c&=d\\tag*{two}\\end{align}"));
        check(static_cast<bool>(*taggedRows), "aligned rows retain independent authored tags");
        if (*taggedRows)
        {
            const auto& matrix = taggedRows->root.children[0u];
            check(matrix.kind == MathKind::Matrix && matrix.columns == 2u && matrix.children.size() == 4u && matrix.children[0u].text == L"rowtag" &&
                      matrix.children[2u].text == L"rowtag" && matrix.children[0u].children[1u].kind == MathKind::Row &&
                      matrix.children[2u].children[1u].text == L"two",
                  "tags belong to distinct rows and starred syntax omits generated parentheses");
            Detail::PreparedMath tagged(factory, taggedRows, 24.f, true);
            while (! tagged.Advance())
            {
            }
            const auto& anchors = tagged.RowTags();
            check(anchors.size() == 2u && anchors[0u].baseline < anchors[1u].baseline, "prepared row tag anchors follow the actual matrix baselines");
            if (anchors.size() == 2u)
            {
                auto firstLabel  = tagged.PrepareRowTag(0u);
                auto secondLabel = tagged.PrepareRowTag(1u);
                while (! firstLabel->Advance())
                {
                }
                while (! secondLabel->Advance())
                {
                }
                check(firstLabel->GlyphLayouts().size() == 3u && secondLabel->GlyphLayouts().size() == 1u,
                      "row labels prepare independently with authored starred parentheses policy");
            }
            bool inlineRefused{};
            try
            {
                Detail::PreparedMath inlineTag(factory, taggedRows, 24.f, false);
            }
            catch (const std::length_error&)
            {
                inlineRefused = true;
            }
            check(inlineRefused, "row tags refuse inline preparation");
            auto invalidLabel                                              = std::make_shared<MathExpression>(*taggedRows);
            invalidLabel->root.children[0u].children[0u].children[1u].kind = MathKind::Accent;
            bool invalidRefused{};
            try
            {
                Detail::PreparedMath invalid(factory, invalidLabel, 24.f, true);
            }
            catch (const std::length_error&)
            {
                invalidRefused = true;
            }
            check(invalidRefused, "direct row-tag data rejects unsupported non-text label shapes");
            auto nestedTag = std::make_shared<MathExpression>(*taggedRows);
            auto wrapper   = nestedTag->root;
            wrapper.children.clear();
            wrapper.children.push_back(nestedTag->root.children[0u]);
            nestedTag->root.children[0u] = std::move(wrapper);
            bool nestedRefused{};
            try
            {
                Detail::PreparedMath invalid(factory, nestedTag, 24.f, true);
            }
            catch (const std::length_error&)
            {
                nestedRefused = true;
            }
            check(nestedRefused, "direct nested matrix data cannot introduce row tags outside a root environment");
        }
        std::wstring fullTags(L"\\begin{gather}");
        for (unsigned row = 0u; row < MathMaximumRows; ++row)
        {
            if (row)
                fullTags += L"\\\\";
            fullTags += L"a\\tag{" + std::to_wstring(row) + L"}";
        }
        fullTags += L"\\end{gather}";
        const auto fullTaggedExpression = std::make_shared<const MathExpression>(ParseMath(fullTags));
        check(static_cast<bool>(*fullTaggedExpression), "all 64 matrix rows may independently retain their tags");
        if (*fullTaggedExpression)
        {
            Detail::PreparedMath fullTagged(factory, fullTaggedExpression, 24.f, true);
            while (! fullTagged.Advance())
            {
            }
            check(fullTagged.RowTags().size() == MathMaximumRows && fullTagged.RowTags().front().baseline < fullTagged.RowTags().back().baseline,
                  "64 row-tag anchors retain source order and distinct matrix baselines");
        }
        check(! ParseMath(L"\\begin{gather}a\\tag{1}\\tag{2}\\end{gather}") && ! ParseMath(L"\\begin{align}a\\tag{1}&b\\tag{2}\\end{align}") &&
                  ! ParseMath(L"\\begin{align}{a\\tag{1}}&b\\end{align}") && ! ParseMath(L"\\tag{outer}\\begin{gather}a\\tag{inner}\\end{gather}") &&
                  ! ParseMath(L"\\begin{matrix}a\\tag{1}\\end{matrix}"),
              "duplicate row tags and unsupported environments retain exact source");
        check(static_cast<bool>(ParseMath(L"\\begin{align}\\displaystyle a&=b\\tag{1}\\end{align}")),
              "row tags survive scoped math-style declarations in their own cell");
    }
#endif
    for (const auto source : {L"\\genfrac{(}{)}{-2pt}{0}{a}{b}", L"\\genfrac{(}{)}{}{4}{a}{b}", L"\\genfrac{?}{}{}{}{a}{b}", L"\\genfrac{}{}{}{}{a}"})
        check(! ParseMath(source), "unsupported general fraction dimensions, style and delimiters reject the whole source");
    {
        const auto expression = ParseMath(L"\\begin{alignat*}{2}a&b&c&d\\end{alignat*}");
        check(expression && expression.root.children[0u].text == L"alignat" && expression.root.children[0u].columns == 4u,
              "alignat retains its declared number of alternating alignment pairs");
#if DXUI_ENABLE_DIAGNOSTICS
        Detail::PreparedMath tight(factory, std::make_shared<const MathExpression>(expression), 24.f);
        Detail::PreparedMath normal(factory, std::make_shared<const MathExpression>(ParseMath(L"\\begin{aligned}a&b&c&d\\end{aligned}")), 24.f);
        while (! tight.Advance())
        {
        }
        while (! normal.Advance())
        {
        }
        check(std::abs(normal.Size().width - tight.Size().width - 3.f * 24.f * .65f) < .01f,
              "alignat adds no automatic gaps between columns, unlike aligned's three existing gaps");
#endif
        check(! ParseMath(L"\\begin{alignat}{1}a&b&c\\end{alignat}") && ! ParseMath(L"\\begin{alignat}{0}a\\end{alignat}"),
              "alignat rejects too many columns or a zero pair count");
    }
#if DXUI_ENABLE_DIAGNOSTICS
    {
        Detail::PreparedMath normal(factory, std::make_shared<const MathExpression>(ParseMath(L"\\begin{cases}\\frac{a}{b}\\end{cases}")), 24.f);
        Detail::PreparedMath display(factory, std::make_shared<const MathExpression>(ParseMath(L"\\begin{dcases}\\frac{a}{b}\\end{dcases}")), 24.f);
        Detail::PreparedMath reverse(factory, std::make_shared<const MathExpression>(ParseMath(L"\\begin{rcases}\\frac{a}{b}\\end{rcases}")), 24.f);
        while (! normal.Advance())
        {
        }
        while (! display.Advance())
        {
        }
        while (! reverse.Advance())
        {
        }
        check(display.GlyphLayouts()[0u].font > normal.GlyphLayouts()[0u].font && normal.GlyphLayouts()[0u].origin.x > reverse.GlyphLayouts()[0u].origin.x,
              "display cases use display-style fractions and reverse cases place the brace after the content");
    }
#endif
    check(static_cast<bool>(ParseMath(L"\\begin{drcases}a&b\\end{drcases}")) && ! ParseMath(L"\\begin{drcases}a&b&c\\end{drcases}"),
          "reverse display cases retain the two-column admission bound");
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto continued = ParseMath(L"\\cfrac{x}{y}");
        check(continued && continued.root.children[0u].kind == MathKind::Fraction && continued.root.children[0u].text == L"cfrac",
              "continued fraction retains its display style and numerator strut identity");
        Detail::PreparedMath withStrut(factory, std::make_shared<const MathExpression>(continued), 24.f);
        Detail::PreparedMath ordinary(factory, std::make_shared<const MathExpression>(ParseMath(L"\\dfrac{x}{y}")), 24.f);
        while (! withStrut.Advance())
        {
        }
        while (! ordinary.Advance())
        {
        }
        check(withStrut.Size().height >= ordinary.Size().height && withStrut.GlyphLayouts().size() == 2u && withStrut.StrokeLayouts().size() == 1u,
              "continued fraction reserves its numerator strut without losing its rule or operands");
    }
#endif
    check(! ParseMath(L"\\cfrac[l]{x}{y}"), "unimplemented continued-fraction alignment retains exact source");
    for (const auto name : {L"matrix", L"pmatrix", L"bmatrix", L"Bmatrix", L"vmatrix", L"Vmatrix"})
    {
        const auto source     = L"\\begin{" + std::wstring(name) + L"*}[r]a\\\\aa\\end{" + std::wstring(name) + L"*}";
        const auto expression = ParseMath(source);
        check(expression && expression.root.children[0u].text == std::wstring(name) + L":r", "starred matrix retains its alignment and fence family");
#if DXUI_ENABLE_DIAGNOSTICS
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        const auto glyphs = layout.GlyphLayouts();
        check(glyphs.size() == 2u && std::abs(glyphs[0u].origin.x + glyphs[0u].extent.width - glyphs[1u].origin.x - glyphs[1u].extent.width) < .01f,
              "starred matrix right-aligns independently shaped cells");
#endif
    }
    check(! ParseMath(L"\\begin{matrix*}[z]a\\end{matrix*}"), "unsupported starred matrix alignment rejects to source");
    std::wstring nestedArrows(0u, L' '), nestedGeneral(0u, L' ');
    for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
    {
        nestedArrows += L"\\xrightarrow";
        nestedGeneral += L"\\genfrac{}{}{}{}";
    }
    nestedArrows += L" x";
    nestedGeneral += L'x';
    nestedGeneral.append(MathMaximumGroupDepth, L'y');
    check(static_cast<bool>(ParseMath(nestedArrows)), "64 nested extensible arrows fit the ordinary thread stack");
    check(static_cast<bool>(ParseMath(nestedGeneral)), "64 nested general fractions fit the ordinary thread stack");
    std::wstring nestedArrays(0u, L' ');
    for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
        nestedArrays += L"\\begin{array}{c}";
    nestedArrays += L'x';
    for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
        nestedArrays += L"\\end{array}";
    check(static_cast<bool>(ParseMath(nestedArrays)), "64 nested arrays fit the ordinary thread stack");
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto nestedArrayExpression = std::make_shared<const MathExpression>(ParseMath(nestedArrays));
        Detail::PreparedMath nestedArrayGeometry(factory, nestedArrayExpression, 12.f);
        while (! nestedArrayGeometry.Advance())
        {
        }
        check(nestedArrayGeometry.GlyphLayouts().size() == 1u, "64 nested arrays also prepare geometry on the ordinary stack");
    }
#endif
    {
        std::wstring nestedFractions(0u, L' ');
        for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
            nestedFractions += L"\\frac";
        nestedFractions += L" x";
        nestedFractions.append(MathMaximumGroupDepth, L'y');
        check(static_cast<bool>(ParseMath(nestedFractions)), "64 nested fractions fit the ordinary parser stack");
#if DXUI_ENABLE_DIAGNOSTICS
        const auto nestedFractionExpression = std::make_shared<const MathExpression>(ParseMath(nestedFractions));
        Detail::PreparedMath nestedFractionGeometry(factory, nestedFractionExpression, 12.f);
        while (! nestedFractionGeometry.Advance())
        {
        }
        check(nestedFractionGeometry.GlyphLayouts().size() == MathMaximumGroupDepth + 1u, "64 nested fractions also prepare geometry on the ordinary stack");
#endif
    }
    for (const auto environment : {L"darray", L"subarray"})
    {
        std::wstring deep(0u, L' ');
        for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
            deep += L"\\begin{" + std::wstring(environment) + L"}{c}";
        deep += L'x';
        for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
            deep += L"\\end{" + std::wstring(environment) + L"}";
        check(static_cast<bool>(ParseMath(deep)), "64 nested styled arrays fit the ordinary thread stack");
    }
    for (const auto command : {L"\\binom", L"\\overset"})
    {
        std::wstring nested(0u, L' ');
        for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
            nested += command;
        nested += L" x";
        nested.append(MathMaximumGroupDepth, L'y');
        check(static_cast<bool>(ParseMath(nested)), "64 nested structured binary commands fit the ordinary parser stack");
#if DXUI_ENABLE_DIAGNOSTICS
        const auto nestedExpression = std::make_shared<const MathExpression>(ParseMath(nested));
        Detail::PreparedMath nestedGeometry(factory, nestedExpression, 12.f);
        while (! nestedGeometry.Advance())
        {
        }
        check(nestedGeometry.GlyphLayouts().size() == MathMaximumGroupDepth + 1u,
              "64 nested structured binary commands also prepare geometry on the ordinary stack");
#endif
    }
    std::wstring nestedSwitches(0u, L' ');
    for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
        nestedSwitches += L"\\bf ";
    nestedSwitches += L'x';
    check(static_cast<bool>(ParseMath(nestedSwitches)), "64 scoped font declarations fit the ordinary thread stack");
    std::wstring nestedSans(0u, L' ');
    for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
        nestedSans += L"\\sf ";
    nestedSans += L'x';
    check(static_cast<bool>(ParseMath(nestedSans)), "64 nested remapping declarations fit the ordinary thread stack");
    for (const auto command : {L"\\hat ", L"\\overleftarrow ", L"\\boxed ", L"\\overbrace ", L"\\sqrt ", L"\\mathcal "})
    {
        std::wstring deep(0u, L' ');
        for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
            deep += command;
        deep += L'x';
        check(static_cast<bool>(ParseMath(deep)), "64-level unary command family retains the ordinary parser stack");
    }
    {
        std::wstring mixed(0u, L' ');
        for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
            mixed += i % 2u ? L"\\overleftarrow " : L"\\sqrt ";
        mixed += L'x';
        check(static_cast<bool>(ParseMath(mixed)), "mixed unary nesting retains the ordinary parser stack");
    }
    {
        std::wstring deep(0u, L' ');
        for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
            deep += L"\\textbf{";
        deep += L'x';
        deep.append(MathMaximumGroupDepth, L'}');
        check(static_cast<bool>(ParseMath(deep)), "64 nested text font groups retain the ordinary parser stack");
    }
    {
        std::wstring scriptDepth(L"\\mathscr{");
        scriptDepth.append(MathMaximumGroupDepth - 2u, L'{');
        scriptDepth += L'A';
        scriptDepth.append(MathMaximumGroupDepth - 1u, L'}');
        check(static_cast<bool>(ParseMath(scriptDepth)), "script command and its 63 groups fit the ordinary stack at their admitted nesting boundary");
    }
    {
        std::wstring deepLeft(0u, L' '), deepNot(0u, L' ');
        for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
        {
            deepLeft += L"\\left(";
            deepNot += L"\\not ";
        }
        deepLeft += L'x';
        deepNot += L'=';
        for (unsigned i = 0u; i < MathMaximumGroupDepth; ++i)
            deepLeft += L"\\right)";
        const auto delimited = std::make_shared<const MathExpression>(ParseMath(deepLeft));
        check(static_cast<bool>(*delimited), "64 nested paired delimiters fit the ordinary parser stack");
        check(! ParseMath(deepNot), "deep unsupported nested negation refuses without overflow");
#if DXUI_ENABLE_DIAGNOSTICS
        Detail::PreparedMath delimitedGeometry(factory, delimited, 12.f);
        while (! delimitedGeometry.Advance())
        {
        }
        check(delimitedGeometry.GlyphLayouts().size() == 1u, "64 nested paired delimiters prepare on the ordinary stack");
#endif
    }
#if DXUI_ENABLE_DIAGNOSTICS
    const auto lowestObservedStack         = Detail::testRichMathLowestStackAddress;
    Detail::testRichMathLowestStackAddress = 0u;
    check(lowestObservedStack > stackLow && lowestObservedStack < stackHigh && lowestObservedStack - stackLow >= 128u * 1024u,
          "64-level parser and placement cases retain at least 128 KiB of observed thread-stack headroom");
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto expression = ParseMath(L"\\begin{array}{|l|r|}a&bb\\end{array}");
        check(expression && expression.root.children[0u].text == L"array:lr:|||", "array retains all three declared vertical boundaries");
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        const auto lines = layout.StrokeLayouts();
        check(lines.size() == 3u && lines[0u].first.x < lines[1u].first.x && lines[1u].first.x < lines[2u].first.x && lines[0u].first.x == lines[0u].last.x &&
                  lines[1u].first.x == lines[1u].last.x && lines[2u].first.x == lines[2u].last.x,
              "array prepares ordered vertical column rules without folding them into cell text");
    }
#endif
    check(! ParseMath(L"\\begin{array}{l||r}a&b\\end{array}"), "unimplemented repeated column rules preserve source");
#if DXUI_ENABLE_DIAGNOSTICS
    {
        Detail::PreparedMath empty(factory, std::make_shared<const MathExpression>(ParseMath(L"\\begin{array}{|c|}\\end{array}")), 24.f);
        while (! empty.Advance())
        {
        }
        check(empty.StrokeLayouts().size() == 2u, "empty array column safely retains both declared rules");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto expression = ParseMath(L"\\begin{array}{|l|r|}\\hline a&b\\\\\\hline c&d\\\\\\hline\\end{array}");
        check(expression && expression.root.children[0u].text == L"array:lr:|||:---", "array records each row and column rule separately");
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        const auto lines = layout.StrokeLayouts();
        check(lines.size() == 6u && lines[0u].first.y == lines[0u].last.y && lines[1u].first.y == lines[1u].last.y && lines[2u].first.y == lines[2u].last.y &&
                  lines[0u].first.y < lines[1u].first.y && lines[1u].first.y < lines[2u].first.y,
              "array stages top, internal and bottom horizontal rules in source order");
    }
#endif
    check(! ParseMath(L"\\begin{array}{c}\\hline\\hline x\\end{array}"), "unimplemented duplicate array rules retain source");
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto expression = ParseMath(L"\\begin{array}{:l:r:}\\hdashline a&bb\\\\\\hdashline\\end{array}");
        check(static_cast<bool>(expression), "dashed column and row rules are admitted");
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        unsigned horizontal{}, vertical{};
        for (const auto& line : layout.StrokeLayouts())
        {
            horizontal += line.first.y == line.last.y;
            vertical += line.first.x == line.last.x;
        }
        check(horizontal >= 2u && vertical >= 3u, "dashed rules stage bounded separate horizontal and vertical segments");
    }
#endif
    {
        const auto nested = ParseMath(L"\\begin{matrix}\\begin{matrix}a\\\\[1em]b\\end{matrix}\\\\[2pt]c\\end{matrix}");
        check(static_cast<bool>(nested), "nested matrix row-spacing scratch remains independent by depth");
        if (nested)
        {
            const auto& outer = nested.root.children[0u];
            check(outer.text == L"matrix;gaps:2pt,." && outer.children[0u].children[0u].text == L"matrix;gaps:1em,.",
                  "nested matrix gaps remain attached to their authored rows");
        }
    }
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto left     = std::make_shared<const MathExpression>(ParseMath(L"\\begin{subarray}{l}a\\\\abcdef\\end{subarray}"));
        const auto centered = std::make_shared<const MathExpression>(ParseMath(L"\\begin{subarray}{c}a\\\\abcdef\\end{subarray}"));
        check(static_cast<bool>(*left) && static_cast<bool>(*centered), "subarray admits its bounded single alignment column");
        Detail::PreparedMath leftLayout(factory, left, 24.f);
        Detail::PreparedMath centeredLayout(factory, centered, 24.f);
        while (! leftLayout.Advance())
        {
        }
        while (! centeredLayout.Advance())
        {
        }
        const auto leftGlyphs   = leftLayout.GlyphLayouts();
        const auto centerGlyphs = centeredLayout.GlyphLayouts();
        check(leftGlyphs.size() == 2u && centerGlyphs.size() == 2u && std::abs(leftGlyphs[0u].origin.x - leftGlyphs[1u].origin.x) < .01f &&
                  centerGlyphs[0u].origin.x > centerGlyphs[1u].origin.x && leftGlyphs[0u].font < 24.f,
              "subarray stages script-size cells with independently verified left or centered alignment");
        check(! ParseMath(L"\\begin{subarray}{r}a\\end{subarray}") && ! ParseMath(L"\\begin{subarray}{l}a&b\\end{subarray}"),
              "unsupported subarray columns retain complete source");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto ordinary = std::make_shared<const MathExpression>(ParseMath(L"\\begin{array}{c}\\sum_{i}^{n}\\end{array}"));
        const auto display  = std::make_shared<const MathExpression>(ParseMath(L"\\begin{darray}{c}\\sum_{i}^{n}\\end{darray}"));
        check(static_cast<bool>(*ordinary) && static_cast<bool>(*display), "darray admits the array column grammar");
        Detail::PreparedMath ordinaryLayout(factory, ordinary, 24.f);
        Detail::PreparedMath displayLayout(factory, display, 24.f);
        while (! ordinaryLayout.Advance())
        {
        }
        while (! displayLayout.Advance())
        {
        }
        check(displayLayout.Size().height > ordinaryLayout.Size().height && displayLayout.GlyphLayouts().size() == ordinaryLayout.GlyphLayouts().size(),
              "darray uses display-style operator limits rather than array text style");
        check(! ParseMath(L"\\begin{darray}{z}a\\end{darray}"), "invalid darray columns retain source");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto alignedat = std::make_shared<const MathExpression>(ParseMath(L"\\begin{alignedat}{2}a&b&c&d\\end{alignedat}"));
        const auto alignat   = std::make_shared<const MathExpression>(ParseMath(L"\\begin{alignat}{2}a&b&c&d\\end{alignat}"));
        check(static_cast<bool>(*alignedat) && static_cast<bool>(*alignat), "alignedat accepts bounded explicit column pairs");
        Detail::PreparedMath embedded(factory, alignedat, 24.f);
        Detail::PreparedMath display(factory, alignat, 24.f);
        while (! embedded.Advance())
        {
        }
        while (! display.Advance())
        {
        }
        check(std::abs(embedded.Size().width - display.Size().width) < .01f && embedded.GlyphLayouts().size() == 4u,
              "alignedat shares explicit no-gap pair geometry");
        check(! ParseMath(L"\\begin{alignedat}{0}a\\end{alignedat}") && ! ParseMath(L"\\begin{alignedat}{1}a&b&c\\end{alignedat}"),
              "invalid alignedat pair bounds retain source");
    }
#endif
    for (const auto source :
         {L"\\begin{matrix}a\\\\[-1pt]b\\end{matrix}", L"\\substack{a\\\\[65pt]b}", L"\\begin{matrix}a\\\\[bogus]b\\end{matrix}", L"\\substack{a\\\\[2ptb}"})
        check(! ParseMath(source), "invalid row-spacing dimensions retain complete source");
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto& [plainSource, spacedSource] : {std::pair{L"\\begin{matrix}a\\\\b\\end{matrix}", L"\\begin{matrix}a\\\\[2pt]b\\end{matrix}"},
                                                    std::pair{L"\\substack{a\\\\b}", L"\\substack{a\\\\[2pt]b}"}})
    {
        const auto plainExpression  = std::make_shared<const MathExpression>(ParseMath(plainSource));
        const auto spacedExpression = std::make_shared<const MathExpression>(ParseMath(spacedSource));
        check(static_cast<bool>(*plainExpression) && static_cast<bool>(*spacedExpression), "bounded matrix row spacing parses");
        Detail::PreparedMath plain(factory, plainExpression, 24.f);
        Detail::PreparedMath spaced(factory, spacedExpression, 24.f);
        Detail::PreparedMath zoomed(factory, spacedExpression, 24.f, false, nullptr, 96.f, {}, 2.f);
        while (! plain.Advance())
        {
        }
        while (! spaced.Advance())
        {
        }
        while (! zoomed.Advance())
        {
        }
        const auto distance = [](const auto& layout)
        {
            const auto glyphs = layout.GlyphLayouts();
            return glyphs.size() == 2u ? glyphs[1u].origin.y - glyphs[0u].origin.y : -1000.f;
        };
        check(std::abs(distance(spaced) - distance(plain) - 8.f / 3.f) < .01f && std::abs(distance(zoomed) - distance(plain) - 16.f / 3.f) < .01f,
              "row-spacing point dimensions separate actual glyph origins and follow absolute zoom");
    }
#endif
    {
        const auto rows     = ParseMath(L"\\begin{matrix}a\\cr b\\end{matrix}");
        const auto substack = ParseMath(L"\\substack{a\\cr b}");
        check(rows && rows.root.children[0u].children.size() == 2u && substack && substack.root.children[0u].children.size() == 2u,
              "cr separates rows in matrices and substacks");
        check(! ParseMath(L"a\\cr b") && ! ParseMath(L"\\begin{matrix}a\\cr[2pt]b\\end{matrix}") && ! ParseMath(L"\\begin{matrix}a\\crammed b\\end{matrix}"),
              "row separator remains scoped, refuses unsupported spacing and cannot consume longer commands");
    }
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto& [source, expected] :
         {std::pair{L"a\\above 2pt b", 8.f / 3.f}, std::pair{L"\\genfrac{}{}{.25em}{}{a}{b}", 6.f}, std::pair{L"a\\above 0pt b", 0.f}})
    {
        const auto expression = ParseMath(source);
        check(static_cast<bool>(expression), "bounded explicit fraction rule dimension parses");
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        const auto lines = layout.StrokeLayouts();
        check(expected == 0.f ? lines.empty() : lines.size() == 1u && std::abs(lines[0u].thickness - expected) < .001f,
              "explicit point and em rule thickness is retained in staged geometry");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        wil::com_ptr<IDWriteFontCollection> collection;
        THROW_IF_FAILED(factory->GetSystemFontCollection(collection.put()));
        UINT32 familyIndex{};
        BOOL found{};
        THROW_IF_FAILED(collection->FindFamilyName(L"Cambria Math", &familyIndex, &found));
        check(found, "ex rule oracle requires the system math font");
        wil::com_ptr<IDWriteFontFamily> mathFamily;
        THROW_IF_FAILED(collection->GetFontFamily(familyIndex, mathFamily.put()));
        wil::com_ptr<IDWriteFont> font;
        THROW_IF_FAILED(mathFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, font.put()));
        DWRITE_FONT_METRICS metrics{};
        font->GetMetrics(&metrics);
        const float exRatio   = metrics.designUnitsPerEm && metrics.xHeight ? static_cast<float>(metrics.xHeight) / metrics.designUnitsPerEm : .45f;
        const auto expression = ParseMath(L"a\\above .5ex b");
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        check(layout.StrokeLayouts().size() == 1u && std::abs(layout.StrokeLayouts()[0u].thickness - 12.f * exRatio) < .001f,
              "ex rule thickness uses the actual resolved font's x-height");
    }
#endif
    check(! ParseMath(L"a\\above -1pt b") && ! ParseMath(L"a\\above 65pt b") && ! ParseMath(L"a\\above 1zz b"),
          "negative, oversized and unsupported rule dimensions fail closed");
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto points   = std::make_shared<const MathExpression>(ParseMath(L"a\\above 2pt b"));
        const auto relative = std::make_shared<const MathExpression>(ParseMath(L"a\\above .25em b"));
        Detail::PreparedMath base(factory, points, 24.f);
        Detail::PreparedMath zoomed(factory, points, 24.f, false, nullptr, 96.f, {}, 2.f);
        Detail::PreparedMath emZoomed(factory, relative, 24.f, false, nullptr, 96.f, {}, 2.f);
        while (! base.Advance())
        {
        }
        while (! zoomed.Advance())
        {
        }
        while (! emZoomed.Advance())
        {
        }
        check(std::abs(base.StrokeLayouts()[0u].thickness - 8.f / 3.f) < .001f && std::abs(zoomed.StrokeLayouts()[0u].thickness - 16.f / 3.f) < .001f &&
                  std::abs(emZoomed.StrokeLayouts()[0u].thickness - 6.f) < .001f,
              "absolute unit scale changes point rules without rescaling font-relative em rules");
        for (const float invalid : {0.f, -1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        {
            bool scaleRejected{};
            try
            {
                Detail::PreparedMath bad(factory, points, 24.f, false, nullptr, 96.f, {}, invalid);
            }
            catch (const std::invalid_argument&)
            {
                scaleRejected = true;
            }
            check(scaleRejected, "invalid absolute-unit scale rejects before preparation");
        }
    }
#endif
    for (const auto source : {L"\\text{caf\\'e}", L"\\text{caf\\'{e}}"})
    {
        const auto expression = ParseMath(source);
        check(expression && expression.root.children[0u].text == L"cafe\u0301", "text accents retain the base and combining mark");
#if DXUI_ENABLE_DIAGNOSTICS
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        check(layout.GlyphLayouts().size() == 1u && layout.GlyphLayouts()[0u].node->text == L"cafe\u0301" && layout.Size().width > 0.f,
              "DirectWrite receives the composed text sequence during preparation");
#endif
    }
    {
        const auto expression = ParseMath(L"\\colorbox{red}{caf\\'{e}}");
        check(expression && expression.root.children[0u].children[0u].text == L"cafe\u0301", "colored text reuses text-accent decoding");
    }
    for (const auto source : {L"\\text{\\'{ab}}", L"\\text{\\'}", L"\\text{\\cafe}"})
        check(! ParseMath(source), "unsupported or incomplete text accents preserve the complete source");
    for (const auto source : {L"\\genfrac ( ] {2pt}{1}a{a+1}", L"{a\\above{2pt} b+1}", L"\\genfrac\\langle\\rangle{0pt}{}{a}{b}"})
        check(static_cast<bool>(ParseMath(source)), "pinned KaTeX general-fraction delimiter and grouped-dimension forms parse completely");
#if DXUI_ENABLE_DIAGNOSTICS
    {
        auto familyFormats = std::make_shared<Detail::MathTextFormats>(factory, L"Cambria Math", L"en-us", L"Arial", L"Consolas");
        for (const auto& [command, familyName] : {std::pair{L"textsf", L"Arial"}, std::pair{L"texttt", L"Consolas"}, std::pair{L"textrm", L"Cambria Math"}})
        {
            const auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\" + std::wstring(command) + L"{Ab0}"));
            check(static_cast<bool>(*expression), "bounded text-family command parses");
            Detail::PreparedMath layout(factory, expression, 24.f, false, nullptr, 96.f, familyFormats);
            while (! layout.Advance())
            {
            }
            const auto& glyph = layout.GlyphLayouts()[0u];
            UINT32 length{};
            THROW_IF_FAILED(glyph.layout->GetFontFamilyNameLength(0u, &length));
            std::wstring actual(length + 1u, L'\0');
            THROW_IF_FAILED(glyph.layout->GetFontFamilyName(0u, actual.data(), static_cast<UINT32>(actual.size())));
            actual.resize(length);
            check(actual == familyName, "DirectWrite layout receives the explicitly configured text family");
        }
        check(familyFormats->Size() >= 3u, "distinct text families do not alias the same cached format");
        check(static_cast<bool>(ParseMath(L"\\colorbox{red}{\\textsf{Ab}}")) && static_cast<bool>(ParseMath(L"\\texttt{caf\\'e}")),
              "colored text and decoded accents preserve explicit family roles");
    }
#endif
    for (const auto command : {L"bf", L"it", L"rm", L"sf", L"tt", L"cal"})
        check(static_cast<bool>(ParseMath(L"{" + std::wstring(L"\\") + command + L" x}")), "bounded legacy math font declaration parses");
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto expression = ParseMath(L"{\\sf x {\\rm y}}");
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        const auto& glyphs = layout.GlyphLayouts();
        check(glyphs.size() == 2u && glyphs[0u].node->text != L"x" && glyphs[1u].node->text == L"y",
              "inner roman declaration overrides the enclosing sans alphabet");
    }
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto expression = ParseMath(L"\\bold{x}y");
        Detail::PreparedMath layout(factory, std::make_shared<const MathExpression>(expression), 24.f);
        while (! layout.Advance())
        {
        }
        check(layout.GlyphLayouts().size() == 2u && layout.GlyphLayouts()[0u].bold && ! layout.GlyphLayouts()[1u].bold,
              "bold alias consumes one argument instead of styling the remaining row");
    }
#endif
    for (const auto source : {L"\\bold{x}y", L"\\frak{A}B", L"\\Bbb{A}B"})
        check(static_cast<bool>(ParseMath(source)), "font aliases accept one bounded argument");
    check(! ParseMath(L"{\\sf x \\text{word}}"), "unimplemented text-family nesting retains source instead of rewriting text glyphs");
#if DXUI_ENABLE_DIAGNOSTICS
    for (const auto& [source, italic] : {std::pair{L"\\emph{x}", true}, std::pair{L"\\mathit{\\emph{x}}", false}})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(source));
        check(static_cast<bool>(*expression), "text emphasis parses");
        Detail::PreparedMath layout(factory, expression, 24.f);
        while (! layout.Advance())
        {
        }
        check(layout.GlyphLayouts().size() == 1u && layout.GlyphLayouts()[0u].italic == italic, "text emphasis toggles inherited italic style");
    }
#endif
    check(static_cast<bool>(ParseMath(L"\\colorbox{red}{\\emph{x}}")), "colored text permits bounded emphasis");
#if DXUI_ENABLE_DIAGNOSTICS
    {
        const auto plainExpression  = std::make_shared<const MathExpression>(ParseMath(L"\\mu"));
        const auto shadowExpression = std::make_shared<const MathExpression>(ParseMath(L"\\pmb{\\mu}"));
        check(static_cast<bool>(*shadowExpression), "poor-man bold parses without replacing the atom class");
        Detail::PreparedMath plain(factory, plainExpression, 24.f), shadow(factory, shadowExpression, 24.f);
        while (! plain.Advance())
        {
        }
        while (! shadow.Advance())
        {
        }
        check(! plain.GlyphLayouts()[0u].pseudoBold && shadow.GlyphLayouts()[0u].pseudoBold && ! shadow.GlyphLayouts()[0u].bold &&
                  shadow.Size().width >= plain.Size().width,
              "poor-man bold reserves shadow ink without substituting a true bold font");
        check(static_cast<bool>(ParseMath(L"\\colorbox{red}{\\pmb{x}}")), "colored text accepts bounded poor-man bold");
    }
#endif
    const auto binomial = ParseMath(L"\\binom{n}{k}");
    check(binomial && binomial.root.children.size() == 1u && binomial.root.children[0u].kind == MathKind::Matrix &&
              binomial.root.children[0u].text == L"pmatrix" && binomial.root.children[0u].columns == 1u && binomial.root.children[0u].children.size() == 2u,
          "binomial is a two-row parenthesized construct with no fraction rule");
    for (const auto source : {L"\\begin{matrix}a&b\\\\c\\end{matrix}", L"\\begin{matrix}a\\\\b&c\\\\\\end{matrix}"})
    {
        const auto ragged = ParseMath(source);
        check(ragged && ragged.root.children.size() == 1u && ragged.root.children[0u].kind == MathKind::Matrix && ragged.root.children[0u].columns == 2u &&
                  ragged.root.children[0u].children.size() == 4u,
              "short matrix rows are padded to the widest row, including before a trailing separator");
    }
    std::wstring ragged = L"\\begin{matrix}";
    for (unsigned column = 0u; column < 64u; ++column)
    {
        if (column)
            ragged += L'&';
        ragged += L'a';
    }
    for (unsigned row = 1u; row < 16u; ++row)
        ragged += L"\\\\a";
    const auto padded = ParseMath(ragged + L"\\end{matrix}");
    check(padded && padded.root.children[0u].children.size() == 1024u, "implicit empty cells are admitted at the exact 1024-cell rectangular bound");
    const auto paddingOverflow = ParseMath(ragged + L"\\\\a\\end{matrix}");
    check(! paddingOverflow && paddingOverflow.issue == MathIssue::Capacity && paddingOverflow.root.children.empty(),
          "generated empty cells reject rectangular overflow before allocating the padded grid");
    for (const auto& [source, canonical] : {std::pair{L"\\begin{align}a&=b\\\\c&=d\\end{align}", L"aligned"},
                                            std::pair{L"\\begin{align*}a&=b\\end{align*}", L"aligned"},
                                            std::pair{L"\\begin{split}a&=b\\end{split}", L"aligned"},
                                            std::pair{L"\\begin{gather}a\\\\b\\end{gather}", L"gathered"},
                                            std::pair{L"\\begin{gather*}a\\end{gather*}", L"gathered"},
                                            std::pair{L"\\begin{equation}a\\end{equation}", L"gathered"},
                                            std::pair{L"\\begin{equation*}a\\end{equation*}", L"gathered"}})
    {
        const auto expression = ParseMath(source);
        check(expression && expression.root.children.size() == 1u && expression.root.children[0u].kind == MathKind::Matrix &&
                  expression.root.children[0u].text == canonical,
              "unnumbered environment aliases preserve their aligned or centered layout rule");
    }
    for (const auto source : {L"\\begin{equation}a\\\\b\\end{equation}", L"\\begin{gather}a&b\\end{gather}", L"\\begin{align}a\\end{align*}"})
        check(! ParseMath(source), "environment aliases preserve row, column and matching-name restrictions");
    for (const auto source : {L"\\substack{i=0\\\\j=1}", L"\\substack{i=0\\\\j=1\\\\}"})
    {
        const auto stacked = ParseMath(source);
        check(stacked && stacked.root.children.size() == 1u && stacked.root.children[0u].kind == MathKind::Matrix &&
                  stacked.root.children[0u].text == L"subarray:c" && stacked.root.children[0u].columns == 1u && stacked.root.children[0u].children.size() == 2u,
              "substack lays out centered one-column rows and tolerates a trailing row separator");
    }
    check(static_cast<bool>(ParseMath(L"\\sum_{\\substack{i=0\\\\j=1}}")) && ! ParseMath(L"\\substack{a&b}") && ! ParseMath(L"\\substack{a\\\\b"),
          "substack composes with operator limits and rejects extra columns and missing braces");
    std::wstring substackLimit = L"\\substack{";
    for (unsigned row = 0u; row < 64u; ++row)
    {
        if (row)
            substackLimit += L"\\\\";
        substackLimit += L'x';
    }
    check(static_cast<bool>(ParseMath(substackLimit + L"}")) && ! ParseMath(substackLimit + L"\\\\x}"),
          "substack admits at most 64 rows without publishing excess geometry");
    for (const auto& [source, style] : {std::pair{L"\\dbinom{n}{k}", L"displaystyle"}, std::pair{L"\\tbinom{n}{k}", L"textstyle"}})
    {
        const auto expression = ParseMath(source);
        check(expression && expression.root.children.size() == 1u && expression.root.children[0u].kind == MathKind::Style &&
                  expression.root.children[0u].text == style && expression.root.children[0u].children[0u].kind == MathKind::Matrix,
              "explicit binomial style applies to the complete construct");
    }
    static_assert(std::size(Detail::NativeMathSymbols) == 411u);
    for (const auto& symbol : Detail::NativeMathSymbols)
    {
        const auto expression = ParseMath(L"\\" + std::wstring(symbol.name));
        check(expression && expression.root.children.size() == 1u && expression.root.children[0u].kind == MathKind::Text &&
                  expression.root.children[0u].text == std::wstring(1u, symbol.glyph) && expression.root.children[0u].atomClass == symbol.atom &&
                  expression.root.children[0u].italic == symbol.italic,
              "pinned upstream mathematical command preserves its Unicode glyph and atom class");
    }
    for (const auto& [source, glyph, atom] : {std::tuple{L"\\prec", L'≺', MathAtomClass::Relation},
                                              std::tuple{L"\\oplus", L'⊕', MathAtomClass::Binary},
                                              std::tuple{L"\\aleph", L'ℵ', MathAtomClass::Ordinary}})
    {
        const auto expression = ParseMath(source);
        check(expression && expression.root.children.size() == 1u && expression.root.children[0u].text == std::wstring(1u, glyph) &&
                  expression.root.children[0u].atomClass == atom,
              "independent representative relation, binary and ordinary symbols match their reviewed identities");
    }
    for (const auto [rows, columns] : {std::pair{64u, 1u}, std::pair{1u, 64u}, std::pair{64u, 16u}, std::pair{16u, 64u}, std::pair{32u, 32u}})
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(Matrix(rows, columns)));
        check(static_cast<bool>(*expression) && expression->root.children[0u].columns == columns &&
                  expression->root.children[0u].children.size() == static_cast<std::size_t>(rows) * columns,
              "matrix admits exact independent row, column and 1024-cell boundaries");
        Detail::PreparedMath geometry(factory, expression, 12.f);
        std::size_t turns{};
        while (! geometry.Advance())
            check(++turns <= 1024u, "maximum matrix shapes at most one admitted cell per advance");
        const auto& glyphs = geometry.GlyphLayouts();
        check(glyphs.size() == static_cast<std::size_t>(rows) * columns && std::isfinite(geometry.Size().width) && std::isfinite(geometry.Size().height) &&
                  geometry.Size().width > 0.f && geometry.Size().height > 0.f,
              "maximum admitted matrix has complete finite prepared geometry");
        for (unsigned row = 0u; row < rows; ++row)
            for (unsigned column = 0u; column < columns; ++column)
            {
                const auto index = static_cast<std::size_t>(row) * columns + column;
                check((! row || glyphs[index].origin.y > glyphs[index - columns].origin.y) &&
                          (! column || glyphs[index].origin.x > glyphs[index - 1u].origin.x),
                      "maximum matrix retains source-ordered row baselines and distinct column positions");
            }
    }
    for (const auto [rows, columns] : {std::pair{65u, 1u}, std::pair{1u, 65u}, std::pair{32u, 33u}})
    {
        const auto capacityFailure = ParseMath(Matrix(rows, columns));
        check(! capacityFailure && capacityFailure.issue == MathIssue::Capacity && capacityFailure.root.children.empty(),
              "matrix rejects first row, column and total-cell overflow independently without partial syntax");
    }
    const auto atNodes   = ParseMath(std::wstring(4095u, L'+'));
    const auto overNodes = ParseMath(std::wstring(4096u, L'+'));
    check(atNodes && atNodes.root.children.size() == 4095u && ! overNodes && overNodes.issue == MathIssue::Capacity && overNodes.root.children.empty(),
          "AST admission includes the root at 4096 nodes and refuses the first excess atom");
    const auto nested    = ParseMath(std::wstring(64u, L'{') + L"x" + std::wstring(64u, L'}'));
    const auto overDepth = ParseMath(std::wstring(65u, L'{') + L"x" + std::wstring(65u, L'}'));
    check(nested && ! overDepth && overDepth.issue == MathIssue::Capacity, "64 parsed group levels are distinct from bounded terminal and script AST wrappers");
    Detail::PreparedMath nestedGeometry(factory, std::make_shared<const MathExpression>(nested), 12.f);
    check(! nestedGeometry.Advance() && nestedGeometry.Advance(), "the exact group-depth boundary also prepares its terminal glyph");
    for (const std::wstring_view command : {L"\\hat ", L"\\sqrt ", L"\\scriptstyle ", L"\\overleftarrow ", L"\\boxed ", L"\\overbrace ", L"\\mathcal "})
    {
        std::wstring source(0u, L' ');
        for (unsigned level = 0u; level < 64u; ++level)
            source.append(command);
        const auto expression = std::make_shared<const MathExpression>(ParseMath(source + L"x"));
        check(static_cast<bool>(*expression), "maximum recursive command nesting fits the ordinary unoptimized thread stack");
        Detail::PreparedMath geometry(factory, expression, 12.f);
        std::size_t turns{};
        while (! geometry.Advance())
            check(++turns == 1u, "maximum unary command nesting prepares its single terminal glyph");
        check(geometry.GlyphLayouts().size() == 1u && std::isfinite(geometry.Size().width) && std::isfinite(geometry.Size().height),
              "maximum recursive commands retain complete finite geometry on the ordinary thread stack");
        const auto excess = ParseMath(source + std::wstring(command) + L"x");
        check(! excess && excess.issue == MathIssue::Capacity && excess.root.children.empty(),
              "the first excess command nesting returns capacity instead of entering an oversized recursive frame");
    }
    check(static_cast<bool>(ParseMath(L"\\text{" + std::wstring(MathMaximumSourceUnits - 7u, L'x') + L"}")) &&
              ! ParseMath(L"\\text{" + std::wstring(MathMaximumSourceUnits - 6u, L'x') + L"}"),
          "equation source admission accepts its exact UTF-16 limit and rejects the next unit");
    check(! ParseMath(std::wstring(1u, wchar_t(0xd800))) && ! ParseMath(std::wstring{L'x', wchar_t(1), L'y'}),
          "invalid UTF-16 and unsupported controls never enter native text shaping");
}
} // namespace NativeMathCorpusTests
