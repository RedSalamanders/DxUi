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
        const UINT32 scalar = L'x';
        UINT16 glyph{};
        THROW_IF_FAILED(face->GetGlyphIndices(&scalar, 1u, &glyph));
        check(glyph != 0u && fontMetrics.designUnitsPerEm != 0u, "SDK reports the actual math glyph and design scale");
        check(Detail::SameMathFontFile(face.get(), face.get()), "same real SDK font identity accepts its own MATH table");
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
        const auto expression             = std::make_shared<const MathExpression>(ParseMath(L"x_x^x"));
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
                  stacked.root.children[0u].text == L"gathered" && stacked.root.children[0u].columns == 1u && stacked.root.children[0u].children.size() == 2u,
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
    for (const std::wstring_view command : {L"\\hat ", L"\\sqrt ", L"\\scriptstyle "})
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
