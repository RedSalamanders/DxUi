#pragma once
#include "Controls/DxUi.NativeMathSymbols.h"
#include "Controls/DxUi.PreparedMath.h"
#include "Support.Tests.MathFontTable.h"
#include <DxUi/RichMath.h>
#include <array>
#include <cmath>
#include <iostream>
#include <tuple>

// Owned independent fixtures for the bounded native profile. The M01..M20
// inputs also appear in the reviewed KaTeX-0.16.11 target inventory; upstream
// TeX/HTML output is neither evaluated nor imported by the renderer.
namespace NativeMathCorpusTests
{
using namespace DxUi::RichText;
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
    {L"unsupported environment", L"\\begin{array}{cc}a&b\\end{array}"},
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
template <typename Check> void Run(IDWriteFactory* factory, Check&& check)
{
    MathFontTableTests::Run(check);
#if DXUI_ENABLE_DIAGNOSTICS
    const auto readsBefore = Detail::testRichMathConstantReads;
    auto cachedExpression  = std::make_shared<const MathExpression>(ParseMath(L"\\left(\\frac{x_i}{y^2}\\right)"));
    Detail::PreparedMath anchor(factory, cachedExpression, 24.f);
    while (! anchor.Advance())
    {
    }
    for (std::size_t i = 1u; i < 128u; ++i)
    {
        Detail::PreparedMath item(factory, cachedExpression, 24.f, false, &anchor);
        while (! item.Advance())
        {
        }
        check(item.Size().width > 0.f && item.Size().height > 0.f, "cached font data prepares every admitted object");
    }
    check(Detail::testRichMathConstantReads == readsBefore + 1u, "128 formulas share one generation-scoped font table snapshot");
#endif
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
        {L"\\begin{array}{lr}a&b\\end{array}", MathIssue::UnsupportedEnvironment},
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
    const auto operators = ParseMath(L"a-b*c~d");
    check(operators && operators.root.children.size() == 7u && operators.root.children[1u].text == L"−" &&
              operators.root.children[1u].atomClass == MathAtomClass::Binary && operators.root.children[3u].text == L"∗" &&
              operators.root.children[3u].atomClass == MathAtomClass::Binary && operators.root.children[5u].kind == MathKind::Space,
          "math-mode ASCII operators use their mathematical glyphs and tilde is spacing");
    const auto prime = ParseMath(L"f''_i");
    check(prime && prime.root.children.size() == 1u && prime.root.children[0u].kind == MathKind::Scripts && prime.root.children[0u].children[1u].text == L"i" &&
              prime.root.children[0u].children[2u].text == L"′′",
          "prime runs are superscripts on the preceding atom and coexist with subscripts");
    check(! ParseMath(L"f'^2"), "a prime followed by a second superscript is rejected rather than misrendered");
    const auto binomial = ParseMath(L"\\binom{n}{k}");
    check(binomial && binomial.root.children.size() == 1u && binomial.root.children[0u].kind == MathKind::Matrix &&
              binomial.root.children[0u].text == L"pmatrix" && binomial.root.children[0u].columns == 1u && binomial.root.children[0u].children.size() == 2u,
          "binomial is a two-row parenthesized construct with no fraction rule");
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
    for (const auto [rows, columns] : {std::pair{16u, 8u}, std::pair{8u, 16u}})
        check(static_cast<bool>(ParseMath(Matrix(rows, columns))), "matrix admits exactly 128 cells within row and column bounds");
    for (const auto [rows, columns] : {std::pair{17u, 1u}, std::pair{1u, 17u}, std::pair{16u, 9u}})
        check(! ParseMath(Matrix(rows, columns)), "matrix rejects row, column and total-cell overflow independently");
    check(static_cast<bool>(ParseMath(L"\\text{" + std::wstring(MathMaximumSourceUnits - 7u, L'x') + L"}")) &&
              ! ParseMath(L"\\text{" + std::wstring(MathMaximumSourceUnits - 6u, L'x') + L"}"),
          "equation source admission accepts its exact UTF-16 limit and rejects the next unit");
    check(! ParseMath(std::wstring(1u, wchar_t(0xd800))) && ! ParseMath(std::wstring{L'x', wchar_t(1), L'y'}),
          "invalid UTF-16 and unsupported controls never enter native text shaping");
}
} // namespace NativeMathCorpusTests
