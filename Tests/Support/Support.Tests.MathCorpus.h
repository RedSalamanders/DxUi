#pragma once
#include "Controls/DxUi.PreparedMath.h"
#include <DxUi/RichMath.h>
#include <array>
#include <cmath>

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
    {L"E01 missing fraction argument", L"\\frac{a}"},
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
    {L"alpha", L"α"},    {L"beta", L"β"},   {L"gamma", L"γ"},  {L"delta", L"δ"},   {L"epsilon", L"ϵ"}, {L"varepsilon", L"ε"}, {L"theta", L"θ"},
    {L"vartheta", L"ϑ"}, {L"phi", L"ϕ"},    {L"varphi", L"φ"}, {L"Upsilon", L"Υ"}, {L"cdot", L"⋅"},    {L"ell", L"ℓ"},        {L"hbar", L"ℏ"},
    {L"lvert", L"|"},    {L"rvert", L"|"},  {L"lVert", L"‖"},  {L"rVert", L"‖"},   {L"lfloor", L"⌊"},  {L"rfloor", L"⌋"},     {L"lceil", L"⌈"},
    {L"rceil", L"⌉"},    {L"coprod", L"∐"}, {L"bigcup", L"⋃"}, {L"bigcap", L"⋂"},  {L"sec", L"sec"},   {L"csc", L"csc"},
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
    for (const auto& scenario : prepared)
    {
        const auto expression = std::make_shared<const MathExpression>(ParseMath(scenario.source));
        check(static_cast<bool>(*expression), "every admitted independent native math fixture parses completely");
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
    for (const auto& symbol : symbols)
    {
        const auto expression = ParseMath(L"\\" + std::wstring(symbol.command));
        check(expression && expression.root.children.size() == 1u && expression.root.children[0u].kind == MathKind::Text &&
                  expression.root.children[0u].text == symbol.value,
              "named mathematical symbols retain independently specified Unicode identity");
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
