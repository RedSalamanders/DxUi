#include "DxUi.NativeMathSymbols.h"
#include "DxUi.NativeColor.h"
#include <DxUi/RichDocument.h>
#include <DxUi/RichMath.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include <optional>
#include <stdexcept>

namespace DxUi::RichText
{
namespace
{
struct MathParseFailure
{
    MathIssue issue;
};
class Parser
{
    std::wstring_view source;
    std::size_t at{}, nodes{};
    std::vector<unsigned> matrixRowWidths;
    std::optional<MathNode> equationTag;
    bool starredTag{};
    static bool Letter(wchar_t c) noexcept
    {
        return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
    }
    void Space()
    {
        while (at < source.size())
        {
            if (iswspace(source[at]))
                ++at;
            else if (source[at] == L'%')
            {
                while (at < source.size() && source[at] != L'\r' && source[at] != L'\n')
                    ++at;
            }
            else
                break;
        }
    }
    [[noreturn]] static void Reject(MathIssue issue = MathIssue::UnsupportedOrMalformed)
    {
        throw MathParseFailure{issue};
    }
    static MathAtomClass Classify(std::wstring_view value) noexcept
    {
        using enum MathAtomClass;
        if (value.size() != 1u)
            return Ordinary;
        const auto c = value[0];
        if (std::wstring_view(L"+-−*∗×÷⋅±∓∪∩∧∨").find(c) != value.npos)
            return Binary;
        if (std::wstring_view(L"=<>:≤≥≠≈≡∼∝→←↔⇒⇐⇔↦∈∉⊂⊆⊃⊇").find(c) != value.npos)
            return Relation;
        if (std::wstring_view(L"([{⌊⌈⟨").find(c) != value.npos)
            return Opening;
        if (std::wstring_view(L")]}⌋⌉⟩").find(c) != value.npos)
            return Closing;
        return c == L',' || c == L';' ? Punctuation : Ordinary;
    }
    MathNode Make(MathKind kind,
                  std::wstring&& text              = std::wstring(0u, L' '),
                  std::vector<MathNode>&& children = std::vector<MathNode>(0u),
                  unsigned columns                 = 0)
    {
        if (++nodes > MathMaximumNodes)
            Reject(MathIssue::Capacity);
        MathNode result{kind, std::move(text), std::move(children), columns};
        if (kind == MathKind::Text)
        {
            result.atomClass = Classify(result.text);
            result.italic    = ! result.text.empty() && (Letter(result.text[0]) || (result.text[0] >= 0x03b1 && result.text[0] <= 0x03f5));
        }
        else if (kind == MathKind::Scripts || kind == MathKind::Style)
            result.atomClass = result.children.empty() ? MathAtomClass::Ordinary : result.children[0].atomClass;
        else if (kind == MathKind::Fraction || kind == MathKind::Matrix || kind == MathKind::Delimited)
            result.atomClass = MathAtomClass::Inner;
        return result;
    }
    static void Alphabet(MathNode& node, std::wstring_view alphabet)
    {
        constexpr std::u32string_view blackboard   = U"𝔸𝔹ℂ𝔻𝔼𝔽𝔾ℍ𝕀𝕁𝕂𝕃𝕄ℕ𝕆ℙℚℝ𝕊𝕋𝕌𝕍𝕎𝕏𝕐ℤ";
        constexpr std::u32string_view calligraphic = U"𝒜ℬ𝒞𝒟ℰℱ𝒢ℋℐ𝒥𝒦ℒℳ𝒩𝒪𝒫𝒬ℛ𝒮𝒯𝒰𝒱𝒲𝒳𝒴𝒵";
        constexpr std::u32string_view lowerScript  = U"𝒶𝒷𝒸𝒹ℯ𝒻ℊ𝒽𝒾𝒿𝓀𝓁𝓂𝓃ℴ𝓅𝓆𝓇𝓈𝓉𝓊𝓋𝓌𝓍𝓎𝓏";
        constexpr std::u32string_view fraktur      = U"𝔄𝔅ℭ𝔇𝔈𝔉𝔊ℌℑ𝔍𝔎𝔏𝔐𝔑𝔒𝔓𝔔ℜ𝔖𝔗𝔘𝔙𝔚𝔛𝔜ℨ";
        const auto upper                           = alphabet == L"mathcal" ? calligraphic : alphabet == L"mathfrak" ? fraktur : blackboard;
        if (node.kind == MathKind::Text)
        {
            std::wstring text(0u, L' ');
            for (wchar_t unit : node.text)
            {
                char32_t scalar = unit;
                if (unit >= L'A' && unit <= L'Z')
                {
                    if (alphabet == L"mathsf")
                        scalar = 0x1D5A0u + unit - L'A';
                    else if (alphabet == L"mathtt")
                        scalar = 0x1D670u + unit - L'A';
                    else
                        scalar = upper[static_cast<std::size_t>(unit - L'A')];
                }
                else if (unit >= L'a' && unit <= L'z')
                    scalar = alphabet == L"mathcal"    ? lowerScript[static_cast<std::size_t>(unit - L'a')]
                             : alphabet == L"mathfrak" ? 0x1D51Eu + unit - L'a'
                             : alphabet == L"mathsf"   ? 0x1D5BAu + unit - L'a'
                             : alphabet == L"mathtt"   ? 0x1D68Au + unit - L'a'
                                                       : 0x1D552u + unit - L'a';
                else if (unit >= L'0' && unit <= L'9')
                {
                    if (alphabet == L"mathbb")
                        scalar = 0x1D7D8u + unit - L'0';
                    else if (alphabet == L"mathsf")
                        scalar = 0x1D7E2u + unit - L'0';
                    else if (alphabet == L"mathtt")
                        scalar = 0x1D7F6u + unit - L'0';
                }
                if (scalar <= 0xFFFF)
                    text += static_cast<wchar_t>(scalar);
                else
                {
                    scalar -= 0x10000;
                    text += static_cast<wchar_t>(0xD800 + (scalar >> 10));
                    text += static_cast<wchar_t>(0xDC00 + (scalar & 1023));
                }
            }
            node.text.swap(text);
        }
        for (auto& child : node.children)
            Alphabet(child, alphabet);
    }
    std::wstring GroupText(bool textMode = false, bool rejectMathDelimiters = false)
    {
        Space();
        if (at == source.size() || source[at++] != L'{')
            Reject();
        std::wstring value(0u, L' ');
        unsigned depth = 1;
        while (at < source.size())
        {
            auto c = source[at++];
            if (c == L'\\')
            {
                if (at == source.size() || std::wstring_view(L"{}%_$&#\\ ").find(source[at]) == source.npos)
                    Reject();
                value += source[at++];
            }
            else if (c == L'{')
            {
                if (++depth > MathMaximumGroupDepth)
                    Reject(MathIssue::Capacity);
                if (! textMode)
                    value += c;
            }
            else if (c == L'}')
            {
                if (--depth == 0)
                    return std::wstring(value);
                if (! textMode)
                    value += c;
            }
            else if (c == L'%')
            {
                while (at < source.size() && source[at] != L'\r' && source[at] != L'\n')
                    ++at;
            }
            else if (rejectMathDelimiters && c == L'$')
                Reject();
            else if (textMode && iswspace(c))
            {
                // TeX source line endings/tabs are text-mode spaces, never
                // additional DirectWrite lines inside an atomic formula.
                if (value.empty() || value.back() != L' ')
                    value += L' ';
            }
            else
                value += c;
        }
        Reject();
    }
    static bool StarredMatrix(std::wstring_view environment) noexcept
    {
        return environment == L"matrix*" || environment == L"pmatrix*" || environment == L"bmatrix*" || environment == L"Bmatrix*" ||
               environment == L"vmatrix*" || environment == L"Vmatrix*";
    }
    wchar_t MatrixAlignment(std::wstring_view environment)
    {
        if (! StarredMatrix(environment))
            return L'c';
        Space();
        if (at == source.size() || source[at] != L'[')
            return L'c';
        ++at;
        Space();
        if (at == source.size() || (source[at] != L'l' && source[at] != L'c' && source[at] != L'r'))
            Reject(MathIssue::UnsupportedCommand);
        const auto alignment = source[at++];
        Space();
        if (at == source.size() || source[at++] != L']')
            Reject(MathIssue::UnsupportedCommand);
        return alignment;
    }
    std::wstring ArraySpecification()
    {
        std::wstring columns(0u, L' '), rules(1u, L'.');
        for (const auto ch : GroupText())
        {
            if (iswspace(ch))
                continue;
            if (ch == L'|')
            {
                if (rules.back() != L'.')
                    Reject(MathIssue::UnsupportedCommand);
                rules.back() = L'|';
                continue;
            }
            if (ch != L'l' && ch != L'c' && ch != L'r')
                Reject(MathIssue::UnsupportedCommand);
            if (columns.size() == MathMaximumColumns)
                Reject(MathIssue::Capacity);
            columns.push_back(ch);
            rules.push_back(L'.');
        }
        if (columns.empty())
            Reject(MathIssue::MissingRequiredArgument);
        if (rules.find(L'|') != std::wstring::npos)
        {
            columns.push_back(L':');
            columns.append(rules);
        }
        return std::wstring(columns);
    }
    MathNode Matrix(std::wstring environment, unsigned depth)
    {
        constexpr std::array names{L"matrix",
                                   L"pmatrix",
                                   L"bmatrix",
                                   L"Bmatrix",
                                   L"vmatrix",
                                   L"Vmatrix",
                                   L"smallmatrix",
                                   L"cases", L"rcases", L"dcases", L"drcases",
                                   L"aligned",
                                   L"gathered",
                                   L"align",
                                   L"align*",
                                   L"split",
                                   L"gather",
                                   L"gather*",
                                   L"equation",
                                   L"equation*",
                                   L"array", L"alignat", L"alignat*", L"matrix*", L"pmatrix*", L"bmatrix*", L"Bmatrix*", L"vmatrix*", L"Vmatrix*"};
        if (std::ranges::find(names, environment) == names.end())
            Reject(MathIssue::UnsupportedEnvironment);
        const wchar_t matrixAlignment = MatrixAlignment(environment);
        std::wstring columnSpec(0u, L' ');
        unsigned declaredColumns{};
        if (environment == L"array")
        {
            columnSpec.assign(ArraySpecification());
            const auto separator = columnSpec.find(L':');
            declaredColumns = static_cast<unsigned>(separator == std::wstring::npos ? columnSpec.size() : separator);
        }        else if (environment == L"alignat" || environment == L"alignat*")
        {
            const auto argument = GroupText();
            unsigned pairs{};
            for (const auto ch : argument)
            {
                if (iswspace(ch))
                    continue;
                if (ch < L'0' || ch > L'9' || pairs > MathMaximumColumns / 2u)
                    Reject(MathIssue::UnsupportedCommand);
                pairs = pairs * 10u + static_cast<unsigned>(ch - L'0');
            }
            if (! pairs || pairs > MathMaximumColumns / 2u)
                Reject(MathIssue::Capacity);
            declaredColumns = 2u * pairs;
        }
        std::vector<MathNode> cells(0u);
        if (matrixRowWidths.empty())
            matrixRowWidths.resize((MathMaximumGroupDepth + 1u) * (MathMaximumRows + 1u));
        auto* const rowWidths = matrixRowWidths.data() + static_cast<std::size_t>(depth) * (MathMaximumRows + 1u);
        std::fill_n(rowWidths, MathMaximumRows + 1u, 0u);
        unsigned columns{}, rowColumns{}, rows{};
        const auto finish = [&]() -> MathNode
        {
            if (rowColumns)
            {
                rowWidths[rows - 1u] |= rowColumns;
                columns              = std::max(columns, rowColumns);
            }
            if (declaredColumns && columns > declaredColumns)
                Reject(MathIssue::Capacity);
            if (declaredColumns)
                columns = declaredColumns;
            const bool gathered = environment == L"gathered" || environment == L"gather" || environment == L"gather*" || environment == L"equation" ||
                                  environment == L"equation*";
            const bool equation = environment == L"equation" || environment == L"equation*";
            if ((gathered && columns != 1u) || (equation && rows != 1u) || ((environment == L"cases" || environment == L"rcases" || environment == L"dcases" || environment == L"drcases") && columns > 2u))
                Reject();
            if (static_cast<std::size_t>(rows) * columns > MathMaximumMatrixCells)
                Reject(MathIssue::Capacity);
            if (StarredMatrix(environment))
            {
                environment.pop_back();
                environment.push_back(L':');
                environment.push_back(matrixAlignment);
            }
            else if (environment == L"align" || environment == L"align*" || environment == L"split")
                environment = L"aligned";
            else if (environment == L"alignat*")
                environment = L"alignat";
            else if (environment == L"array")
            {
                environment = L"array:" + columnSpec;
                bool horizontal{};
                for (unsigned row = 0u; row <= rows; ++row)
                    horizontal = horizontal || (rowWidths[row] & 0x100u) != 0u;
                if (horizontal)
                {
                    if (columnSpec.find(L':') == std::wstring::npos)
                    {
                        environment.push_back(L':');
                        environment.append(columns + 1u, L'.');
                    }
                    environment.push_back(L':');
                    for (unsigned row = 0u; row <= rows; ++row)
                        environment.push_back((rowWidths[row] & 0x100u) != 0u ? L'-' : L'.');
                }
            }
            else if (gathered)
                environment = L"gathered";
            if (cells.size() == static_cast<std::size_t>(rows) * columns)
                return Make(MathKind::Matrix, std::move(environment), std::move(cells), columns);
            std::vector<MathNode> rectangular(0u);
            rectangular.reserve(static_cast<std::size_t>(rows) * columns);
            std::size_t index{};
            for (unsigned row = 0u; row < rows; ++row)
            {
                for (unsigned column = 0u; column < (rowWidths[row] & 0xffu); ++column)
                    rectangular.push_back(std::move(cells[index++]));
                for (unsigned column = rowWidths[row] & 0xffu; column < columns; ++column)
                    rectangular.push_back(Make(MathKind::Row));
            }
            return Make(MathKind::Matrix, std::move(environment), std::move(rectangular), columns);
        };
        while (at < source.size())
        {
            Space();
            if (environment == L"array" && ! rowColumns && source.substr(at, 6u) == L"\\hline" &&
                (at + 6u == source.size() || ! Letter(source[at + 6u])))
            {
                if ((rowWidths[rows] & 0x100u) != 0u)
                    Reject(MathIssue::UnsupportedCommand);
                rowWidths[rows] |= 0x100u;
                at += 6u;
                continue;
            }
            if (! rowColumns && ! cells.empty() && source.substr(at, 5) == L"\\end{")
            {
                at += 4;
                if (GroupText() != environment)
                    Reject(MathIssue::MismatchedEnvironment);
                return finish();
            }
            if (! rowColumns && ++rows > MathMaximumRows)
                Reject(MathIssue::Capacity);
            if (rowColumns == MathMaximumColumns || cells.size() == MathMaximumMatrixCells)
                Reject(MathIssue::Capacity);
            cells.push_back(Row(depth + 1, false, true));
            ++rowColumns;
            if (source.substr(at, 5) == L"\\end{")
            {
                at += 4;
                if (GroupText() != environment)
                    Reject(MathIssue::MismatchedEnvironment);
                return finish();
            }
            if (at < source.size() && source[at] == L'&')
            {
                ++at;
                continue;
            }
            if (source.substr(at, 2) != L"\\\\")
                Reject();
            at += 2;
            rowWidths[rows - 1u] |= rowColumns;
            columns              = std::max(columns, rowColumns);
            rowColumns           = 0;
        }
        Reject();
    }
    wchar_t Delimiter()
    {
        Space();
        if (at == source.size())
            Reject();
        if (source[at] == L'\\')
        {
            ++at;
            if (at == source.size())
                Reject();
            if (source[at] == L'|')
            {
                ++at;
                return L'\u2016';
            }
            if (source[at] == L'{' || source[at] == L'}')
                return source[at++];
            const auto first = at;
            while (at < source.size() && Letter(source[at]))
                ++at;
            const auto name = source.substr(first, at - first);
            if (name == L"langle")
                return L'⟨';
            if (name == L"rangle")
                return L'⟩';
            if (name == L"lbrace")
                return L'{';
            if (name == L"rbrace")
                return L'}';
            if (name == L"vert" || name == L"lvert" || name == L"rvert")
                return L'|';
            if (name == L"Vert" || name == L"lVert" || name == L"rVert")
                return L'‖';
            if (name == L"lbrack")
                return L'[';
            if (name == L"rbrack")
                return L']';
            if (name == L"lparen")
                return L'(';
            if (name == L"rparen")
                return L')';
            if (name == L"lt")
                return L'<';
            if (name == L"gt")
                return L'>';
            if (name == L"backslash")
                return L'\\';
            if (name == L"lfloor")
                return L'⌊';
            if (name == L"rfloor")
                return L'⌋';
            if (name == L"lceil")
                return L'⌈';
            if (name == L"rceil")
                return L'⌉';
            if (name == L"uparrow")
                return L'↑';
            if (name == L"downarrow")
                return L'↓';
            if (name == L"updownarrow")
                return L'↕';
            if (name == L"Uparrow")
                return L'⇑';
            if (name == L"Downarrow")
                return L'⇓';
            if (name == L"Updownarrow")
                return L'⇕';
            Reject();
        }
        const auto value = source[at++];
        if (std::wstring_view(L"()[]{}|.⌊⌋⌈⌉⟨⟩‖/<>↑↓↕⇑⇓⇕").find(value) == source.npos)
            Reject();
        return value;
    }
    // Keep recursive parser frames small in unoptimized MSVC builds. Literal
    // symbol tables and unrelated command temporaries belong in separate frames.
    MathNode ExtensibleArrowCommand(std::wstring_view name, unsigned depth)
    {
            Space();
            auto lower = Make(MathKind::Row);
            if (at < source.size() && source[at] == L'[')
            {
                ++at;
                lower = Row(depth + 1u, false, false, false, true);
            }
            auto upper = Atom(depth + 1u);
            auto arrow = Make(MathKind::Accent, std::wstring(name), {std::move(upper), std::move(lower)});
            arrow.atomClass = MathAtomClass::Relation;
            return arrow;

    }
    MathNode GeneralFraction(unsigned depth)
    {
        const auto left = GroupText();
        const auto right = GroupText();
        const auto rule = GroupText();
        const auto style = GroupText();
        const auto delimiter = [](std::wstring_view value) -> wchar_t
        {
            if (value.empty() || value == L".")
                return L'.';
            if (value.size() != 1u || std::wstring_view(L"()[]{}|⟨⟩‖⌊⌋⌈⌉/<>\\↑↓↕⇑⇓⇕").find(value[0u]) == std::wstring_view::npos)
                Reject(MathIssue::UnsupportedCommand);
            return value[0u];
        };
        if (! rule.empty() && rule != L"0pt" && rule != L"0em" && rule != L"0ex" && rule != L"0")
            Reject(MathIssue::UnsupportedCommand);
        if (! style.empty() && style != L"0" && style != L"1" && style != L"2" && style != L"3")
            Reject(MathIssue::UnsupportedCommand);
        const auto leftGlyph = delimiter(left), rightGlyph = delimiter(right);
        auto numerator = Atom(depth + 1u);
        auto denominator = Atom(depth + 1u);
        auto fraction = Make(MathKind::Fraction, rule.empty() ? L"frac" : L"atop", {std::move(numerator), std::move(denominator)});
        if (leftGlyph != L'.' || rightGlyph != L'.')
            fraction = Make(MathKind::Delimited, std::wstring{leftGlyph, rightGlyph}, {std::move(fraction)});
        if (! style.empty())
        {
            const auto name = style == L"0" ? L"displaystyle" : style == L"1" ? L"textstyle" : style == L"2" ? L"scriptstyle" : L"scriptscriptstyle";
            fraction = Make(MathKind::Style, name, {std::move(fraction)});
        }
        return fraction;
    }
    MathNode StructuredCommand(std::wstring_view name, unsigned depth)
    {
        if (name == L"not")
        {
            auto operand = Atom(depth + 1u);
            if (operand.kind != MathKind::Text || operand.atomClass != MathAtomClass::Relation || operand.text.size() != 1u)
                Reject(MathIssue::UnsupportedCommand);
            constexpr std::array pairs{std::pair{L'=', L'≠'},
                                       std::pair{L'<', L'≮'},
                                       std::pair{L'>', L'≯'},
                                       std::pair{L'≤', L'≰'},
                                       std::pair{L'≥', L'≱'},
                                       std::pair{L'∈', L'∉'},
                                       std::pair{L'∋', L'∌'},
                                       std::pair{L'⊂', L'⊄'},
                                       std::pair{L'⊃', L'⊅'},
                                       std::pair{L'⊆', L'⊈'},
                                       std::pair{L'⊇', L'⊉'},
                                       std::pair{L'∼', L'≁'},
                                       std::pair{L'≡', L'≢'},
                                       std::pair{L'≈', L'≉'},
                                       std::pair{L'∣', L'∤'},
                                       std::pair{L'∥', L'∦'}};
            for (const auto& [positive, negative] : pairs)
                if (operand.text[0u] == positive)
                {
                    operand.text[0u] = negative;
                    return operand;
                }
            Reject(MathIssue::UnsupportedCommand);
        }
        if (name == L"cfrac")
        {
            Space();
            if (at < source.size() && source[at] == L'[')
                Reject(MathIssue::UnsupportedCommand);
        }
        if (name == L"frac" || name == L"dfrac" || name == L"tfrac" || name == L"cfrac")
            return Make(MathKind::Fraction, std::wstring(name), {Atom(depth + 1), Atom(depth + 1)});
        if (name == L"binom" || name == L"dbinom" || name == L"tbinom")
        {
            auto binomial = Make(MathKind::Matrix, L"pmatrix", {Atom(depth + 1), Atom(depth + 1)}, 1u);
            if (name == L"binom")
                return binomial;
            return Make(MathKind::Style, name == L"dbinom" ? L"displaystyle" : L"textstyle", {std::move(binomial)});
        }
        if (name == L"overset" || name == L"underset" || name == L"stackrel")
        {
            auto annotation = Atom(depth + 1);
            auto base       = Atom(depth + 1);
            if (base.kind == MathKind::Row && base.children.size() == 1u)
                base.atomClass = base.children[0u].atomClass;
            if (name == L"stackrel")
                base.atomClass = MathAtomClass::Relation;
            base.limits = MathLimits::AboveBelow;
            MathNode sub, super;
            if (name == L"underset")
                sub = std::move(annotation);
            else
                super = std::move(annotation);
            return Make(MathKind::Scripts, L"", {std::move(base), std::move(sub), std::move(super)});
        }
        if (name == L"sqrt")
        {
            Space();
            MathNode index;
            if (at < source.size() && source[at] == L'[')
            {
                ++at;
                index = Row(depth + 1u, false, false, false, true);
            }
            return Make(MathKind::Root, L"", {Atom(depth + 1), std::move(index)});
        }
        if (name == L"substack")
        {
            Space();
            if (at == source.size() || source[at++] != L'{')
                Reject(MathIssue::MissingRequiredArgument);
            std::vector<MathNode> rows(0u);
            while (true)
            {
                if (rows.size() == MathMaximumRows)
                    Reject(MathIssue::Capacity);
                rows.push_back(Row(depth + 1u, false, true, false, false, true));
                if (source.substr(at, 2u) == L"\\\\")
                {
                    at += 2u;
                    Space();
                    if (at < source.size() && source[at] == L'}')
                    {
                        ++at;
                        break;
                    }
                    continue;
                }
                if (at < source.size() && source[at] == L'}')
                {
                    ++at;
                    break;
                }
                Reject();
            }
            return Make(MathKind::Matrix, L"gathered", std::move(rows), 1u);
        }
        if (name == L"begin")
            return Matrix(GroupText(), depth);
        Reject(MathIssue::UnsupportedCommand);
    }
    MathNode StyledCommand(std::wstring_view name, unsigned depth)
    {
        if (name == L"text" || name == L"textbf" || name == L"textit" || name == L"textup" || name == L"textmd" || name == L"textnormal" || name == L"operatorname")
        {
            const bool starred = name == L"operatorname" && at < source.size() && source[at] == L'*';
            if (starred)
                ++at;
            auto result          = Make(MathKind::Text, GroupText(true));
            result.italic        = false;
            result.atomClass     = name == L"operatorname" ? MathAtomClass::Operator : MathAtomClass::Ordinary;
            result.displayLimits = starred;
            if (name == L"textup" || name == L"textmd" || name == L"textnormal")
                return Make(MathKind::Style, name == L"textup" ? L"mathrm" : name == L"textmd" ? L"textmd" : L"mathnormal", {std::move(result)});
            if (name == L"textbf" || name == L"textit")
                return Make(MathKind::Style, name == L"textbf" ? L"mathbf" : L"mathit", {std::move(result)});
            return result;
        }
        if (name == L"mathrm" || name == L"mathbf" || name == L"mathit" || name == L"mathnormal" || name == L"boldsymbol" || name == L"bm" ||
            name == L"mathbb" || name == L"mathcal" || name == L"mathfrak" || name == L"mathsf" || name == L"mathtt" || name == L"mathord" ||
            name == L"mathop" || name == L"mathbin" || name == L"mathrel" || name == L"mathopen" || name == L"mathclose" || name == L"mathpunct" ||
            name == L"mathinner")
        {
            auto body = Atom(depth + 1);
            if (name == L"mathbb" || name == L"mathcal" || name == L"mathfrak" || name == L"mathsf" || name == L"mathtt")
                Alphabet(body, name);
            auto result = Make(MathKind::Style, std::wstring(name), {std::move(body)});
            if (name == L"mathord")
                result.atomClass = MathAtomClass::Ordinary;
            else if (name == L"mathop")
            {
                result.atomClass     = MathAtomClass::Operator;
                result.displayLimits = true;
            }
            else if (name == L"mathbin")
                result.atomClass = MathAtomClass::Binary;
            else if (name == L"mathrel")
                result.atomClass = MathAtomClass::Relation;
            else if (name == L"mathopen")
                result.atomClass = MathAtomClass::Opening;
            else if (name == L"mathclose")
                result.atomClass = MathAtomClass::Closing;
            else if (name == L"mathpunct")
                result.atomClass = MathAtomClass::Punctuation;
            else if (name == L"mathinner")
                result.atomClass = MathAtomClass::Inner;
            return result;
        }
        if (name == L"overbrace" || name == L"underbrace" || name == L"overgroup" || name == L"undergroup")
        {
            auto result = Make(MathKind::Accent, std::wstring(name), {Atom(depth + 1u)});
            result.atomClass = MathAtomClass::Operator;
            result.limits = MathLimits::AboveBelow;
            return result;
        }
        if (name == L"boxed" || name == L"cancel" || name == L"bcancel" || name == L"xcancel" || name == L"sout" || name == L"phase" || name == L"angl")
            return Make(MathKind::Accent, std::wstring(name), {Atom(depth + 1)});
        if (name == L"overline" || name == L"underline" || name == L"bar" || name == L"hat" || name == L"widehat" || name == L"vec" || name == L"dot" ||
            name == L"ddot" || name == L"tilde" || name == L"widetilde" || name == L"check" || name == L"widecheck" ||
            name == L"breve" || name == L"acute" || name == L"grave" || name == L"mathring" || name == L"underbar" || name == L"utilde")
            return Make(MathKind::Accent, std::wstring(name), {Atom(depth + 1)});
        if (name == L"left")
        {
            const auto left = Delimiter();
            auto body       = Row(depth + 1, false, false, true);
            if (source.substr(at, 6) != L"\\right")
                Reject(MathIssue::MissingRightDelimiter);
            at += 6;
            const auto right = Delimiter();
            return Make(MathKind::Delimited, std::wstring{left, right}, {std::move(body)});
        }
        Reject(MathIssue::UnsupportedCommand);
    }
    MathNode SymbolCommand(std::wstring_view name)
    {
        auto sizeName = name;
        if (sizeName.ends_with(L"l") || sizeName.ends_with(L"r"))
            sizeName.remove_suffix(1u);
        if (sizeName == L"big" || sizeName == L"Big" || sizeName == L"bigg" || sizeName == L"Bigg")
        {
            const auto delimiter = Delimiter();
            auto result          = Make(MathKind::Delimiter, std::wstring(1, delimiter));
            result.delimiterSize = sizeName == L"big" ? 1u : sizeName == L"Big" ? 2u : sizeName == L"bigg" ? 3u : 4u;
            result.atomClass     = Classify(result.text);
            return result;
        }
        if (name == L"quad" || name == L"qquad")
            return Make(MathKind::Space, std::wstring(name));
        const auto symbol = std::lower_bound(std::begin(Detail::NativeMathSymbols),
                                             std::end(Detail::NativeMathSymbols),
                                             name,
                                             [](const Detail::NativeMathSymbol& entry, std::wstring_view key) { return entry.name < key; });
        if (symbol != std::end(Detail::NativeMathSymbols) && symbol->name == name)
        {
            auto result      = Make(MathKind::Text, std::wstring(1u, symbol->glyph));
            result.atomClass = symbol->atom;
            result.italic    = symbol->italic;
            return result;
        }
        constexpr std::pair<std::wstring_view, std::wstring_view> symbols[]{{L"alpha", L"α"},
                                                                            {L"beta", L"β"},
                                                                            {L"gamma", L"γ"},
                                                                            {L"delta", L"δ"},
                                                                            {L"epsilon", L"ϵ"},
                                                                            {L"varepsilon", L"ε"},
                                                                            {L"zeta", L"ζ"},
                                                                            {L"eta", L"η"},
                                                                            {L"theta", L"θ"},
                                                                            {L"vartheta", L"ϑ"},
                                                                            {L"iota", L"ι"},
                                                                            {L"kappa", L"κ"},
                                                                            {L"varkappa", L"ϰ"},
                                                                            {L"lambda", L"λ"},
                                                                            {L"mu", L"μ"},
                                                                            {L"nu", L"ν"},
                                                                            {L"xi", L"ξ"},
                                                                            {L"omicron", L"ο"},
                                                                            {L"pi", L"π"},
                                                                            {L"varpi", L"ϖ"},
                                                                            {L"rho", L"ρ"},
                                                                            {L"varrho", L"ϱ"},
                                                                            {L"sigma", L"σ"},
                                                                            {L"varsigma", L"ς"},
                                                                            {L"tau", L"τ"},
                                                                            {L"upsilon", L"υ"},
                                                                            {L"phi", L"ϕ"},
                                                                            {L"varphi", L"φ"},
                                                                            {L"chi", L"χ"},
                                                                            {L"psi", L"ψ"},
                                                                            {L"omega", L"ω"},
                                                                            {L"Gamma", L"Γ"},
                                                                            {L"Delta", L"Δ"},
                                                                            {L"Theta", L"Θ"},
                                                                            {L"Lambda", L"Λ"},
                                                                            {L"Xi", L"Ξ"},
                                                                            {L"Pi", L"Π"},
                                                                            {L"Sigma", L"Σ"},
                                                                            {L"Upsilon", L"Υ"},
                                                                            {L"Phi", L"Φ"},
                                                                            {L"Psi", L"Ψ"},
                                                                            {L"Omega", L"Ω"},
                                                                            {L"sum", L"∑"},
                                                                            {L"prod", L"∏"},
                                                                            {L"coprod", L"∐"},
                                                                            {L"bigcup", L"⋃"},
                                                                            {L"bigcap", L"⋂"},
                                                                            {L"int", L"∫"},
                                                                            {L"iint", L"∬"},
                                                                            {L"iiint", L"∭"},
                                                                            {L"oint", L"∮"},
                                                                            {L"infty", L"∞"},
                                                                            {L"times", L"×"},
                                                                            {L"cdot", L"⋅"},
                                                                            {L"div", L"÷"},
                                                                            {L"pm", L"±"},
                                                                            {L"mp", L"∓"},
                                                                            {L"le", L"≤"},
                                                                            {L"leq", L"≤"},
                                                                            {L"ge", L"≥"},
                                                                            {L"geq", L"≥"},
                                                                            {L"ne", L"≠"},
                                                                            {L"neq", L"≠"},
                                                                            {L"approx", L"≈"},
                                                                            {L"equiv", L"≡"},
                                                                            {L"sim", L"∼"},
                                                                            {L"propto", L"∝"},
                                                                            {L"rightarrow", L"→"},
                                                                            {L"leftarrow", L"←"},
                                                                            {L"leftrightarrow", L"↔"},
                                                                            {L"Rightarrow", L"⇒"},
                                                                            {L"Leftarrow", L"⇐"},
                                                                            {L"Leftrightarrow", L"⇔"},
                                                                            {L"to", L"→"},
                                                                            {L"mapsto", L"↦"},
                                                                            {L"partial", L"∂"},
                                                                            {L"nabla", L"∇"},
                                                                            {L"ell", L"ℓ"},
                                                                            {L"hbar", L"ℏ"},
                                                                            {L"ldots", L"…"},
                                                                            {L"cdots", L"⋯"},
                                                                            {L"vdots", L"⋮"},
                                                                            {L"ddots", L"⋱"},
                                                                            {L"in", L"∈"},
                                                                            {L"notin", L"∉"},
                                                                            {L"subset", L"⊂"},
                                                                            {L"subseteq", L"⊆"},
                                                                            {L"supset", L"⊃"},
                                                                            {L"supseteq", L"⊇"},
                                                                            {L"cup", L"∪"},
                                                                            {L"cap", L"∩"},
                                                                            {L"emptyset", L"∅"},
                                                                            {L"forall", L"∀"},
                                                                            {L"exists", L"∃"},
                                                                            {L"neg", L"¬"},
                                                                            {L"land", L"∧"},
                                                                            {L"lor", L"∨"},
                                                                            {L"langle", L"⟨"},
                                                                            {L"rangle", L"⟩"},
                                                                            {L"lbrace", L"{"},
                                                                            {L"rbrace", L"}"},
                                                                            {L"vert", L"|"},
                                                                            {L"Vert", L"‖"},
                                                                            {L"lvert", L"|"},
                                                                            {L"rvert", L"|"},
                                                                            {L"lVert", L"‖"},
                                                                            {L"rVert", L"‖"},
                                                                            {L"lfloor", L"⌊"},
                                                                            {L"rfloor", L"⌋"},
                                                                            {L"lceil", L"⌈"},
                                                                            {L"rceil", L"⌉"},
                                                                            {L"sin", L"sin"},
                                                                            {L"cos", L"cos"},
                                                                            {L"tan", L"tan"},
                                                                            {L"cot", L"cot"},
                                                                            {L"sec", L"sec"},
                                                                            {L"csc", L"csc"},
                                                                            {L"sinh", L"sinh"},
                                                                            {L"cosh", L"cosh"},
                                                                            {L"log", L"log"},
                                                                            {L"ln", L"ln"},
                                                                            {L"exp", L"exp"},
                                                                            {L"lim", L"lim"},
                                                                            {L"max", L"max"},
                                                                            {L"min", L"min"},
                                                                            {L"det", L"det"},
                                                                            {L"gcd", L"gcd"}};
        for (const auto& [key, value] : symbols)
            if (key == name)
            {
                auto result = Make(MathKind::Text, std::wstring(value));
                constexpr std::array operators{L"sum", L"prod", L"coprod", L"bigcup", L"bigcap", L"int", L"iint", L"iiint", L"oint",
                                               L"sin", L"cos",  L"tan",    L"cot",    L"sec",    L"csc", L"sinh", L"cosh",  L"log",
                                               L"ln",  L"exp",  L"lim",    L"max",    L"min",    L"det", L"gcd"};
                if (std::ranges::find(operators, name) != operators.end())
                {
                    result.atomClass     = MathAtomClass::Operator;
                    result.italic        = false;
                    result.largeOperator = value.size() == 1u;
                    result.displayLimits = result.largeOperator ? name != L"int" && name != L"iint" && name != L"iiint" && name != L"oint"
                                                                : name == L"lim" || name == L"max" || name == L"min";
                }
                return result;
            }
        if (name.empty() && at < source.size())
        {
            const auto escaped = source[at++];
            if (std::wstring_view(L",;:! ").find(escaped) != source.npos)
                return Make(MathKind::Space, std::wstring(1, escaped));
            if (escaped == L'|')
                return Make(MathKind::Text, L"‖");
            if (std::wstring_view(L"{}%_$&#").find(escaped) != source.npos)
                return Make(MathKind::Text, std::wstring(1, escaped));
        }
        Reject(MathIssue::UnsupportedCommand);
    }
    MathNode TextColorBody(unsigned depth)
    {
        if (depth > MathMaximumGroupDepth)
            Reject(MathIssue::Capacity);
        Space();
        if (at == source.size() || source[at++] != L'{')
            Reject(MathIssue::MissingRequiredArgument);
        std::vector<MathNode> children(0u);
        std::wstring text(0u, L' ');
        std::wstring activeColor(0u, L' ');
        const auto append = [&](MathNode&& node)
        {
            if (! activeColor.empty())
                node = Make(MathKind::Style, std::wstring(activeColor), {std::move(node)});
            children.push_back(std::move(node));
        };
        const auto flush = [&]
        {
            if (text.empty())
                return;
            auto leaf = Make(MathKind::Text, std::move(text));
            leaf.italic = false;
            leaf.atomClass = MathAtomClass::Ordinary;
            append(std::move(leaf));
            text.clear();
        };
        while (at < source.size())
        {
            const auto unit = source[at++];
            if (unit == L'}')
            {
                flush();
                if (children.size() == 1u)
                    return std::move(children[0u]);
                return Make(MathKind::Row, L"", std::move(children));
            }
            if (unit == L'$')
                Reject(MathIssue::UnsupportedCommand);
            if (unit == L'{')
            {
                --at;
                flush();
                append(TextColorBody(depth + 1u));
                continue;
            }
            if (unit == L'%')
            {
                while (at < source.size() && source[at] != L'\r' && source[at] != L'\n')
                    ++at;
                continue;
            }
            if (unit == L'\\')
            {
                if (at == source.size())
                    Reject(MathIssue::MissingRequiredArgument);
                if (! Letter(source[at]))
                {
                    if (std::wstring_view(L"{}%_$&#\\ ").find(source[at]) == std::wstring_view::npos)
                        Reject(MathIssue::UnsupportedCommand);
                    text.push_back(source[at++]);
                    continue;
                }
                const auto start = at;
                while (at < source.size() && Letter(source[at]))
                    ++at;
                const auto name = source.substr(start, at - start);
                flush();
                if (name == L"color")
                {
                    const auto color = Detail::ParseNativeColor(GroupText());
                    if (! color)
                        Reject(MathIssue::UnsupportedCommand);
                    activeColor.assign(L"textcolor:");
                    activeColor.append(Detail::EncodeNativeColor(color.value()));
                }
                else if (name == L"textcolor" || name == L"colorbox" || name == L"fcolorbox")
                    append(ColorCommand(name, depth, true));
                else if (name == L"textbf" || name == L"textit" || name == L"textrm")
                    append(Make(MathKind::Style, name == L"textbf" ? L"mathbf" : name == L"textit" ? L"mathit" : L"mathrm",
                                {TextColorBody(depth + 1u)}));
                else
                    Reject(MathIssue::UnsupportedCommand);
                continue;
            }
            if (iswspace(unit))
            {
                if (text.empty() || text.back() != L' ')
                    text.push_back(L' ');
            }
            else
                text.push_back(unit);
        }
        Reject(MathIssue::MissingGroupClose);
    }
    MathNode ColorCommand(std::wstring_view name, unsigned depth, bool textMode = false)
    {
        const auto color = Detail::ParseNativeColor(GroupText());
        if (! color)
            Reject(MathIssue::UnsupportedCommand);
        auto label = std::wstring(name);
        label += L':';
        label += Detail::EncodeNativeColor(color.value());
        if (name == L"fcolorbox")
        {
            const auto background = Detail::ParseNativeColor(GroupText());
            if (! background)
                Reject(MathIssue::UnsupportedCommand);
            label += L':';
            label += Detail::EncodeNativeColor(background.value());
        }
        if (name == L"textcolor")
            return Make(MathKind::Style, std::move(label), {textMode ? TextColorBody(depth + 1u) : Atom(depth + 1u)});
        return Make(MathKind::Accent, std::move(label), {TextColorBody(depth + 1u)});
    }
    void AppendColored(std::vector<MathNode>& output, std::wstring_view label, MathNode&& body)
    {
        if (body.kind != MathKind::Row)
        {
            output.push_back(Make(MathKind::Style, std::wstring(label), {std::move(body)}));
            return;
        }
        for (auto& child : body.children)
        {
            const bool space = child.kind == MathKind::Space;
            if (space)
                output.push_back(std::move(child));
            else
                output.push_back(Make(MathKind::Style, std::wstring(label), {std::move(child)}));
        }
    }
    MathNode Command(unsigned depth)
    {
        const auto first = at;
        while (at < source.size() && Letter(source[at]))
            ++at;
        const auto name                                 = source.substr(first, at - first);
        if (name == L"textcolor" || name == L"colorbox" || name == L"fcolorbox")
            return ColorCommand(name, depth);
        if (name == L"xrightarrow" || name == L"xleftarrow" || name == L"xleftrightarrow" || name == L"xRightarrow" || name == L"xLeftarrow" ||
            name == L"xLeftrightarrow" || name == L"xmapsto" || name == L"xlongequal")
            return ExtensibleArrowCommand(name, depth);
        if (name == L"genfrac")
            return GeneralFraction(depth);
        static constexpr std::wstring_view structured[] = {
            L"not", L"frac", L"dfrac", L"tfrac", L"cfrac", L"binom", L"dbinom", L"tbinom", L"overset", L"underset", L"stackrel", L"sqrt", L"substack", L"begin"};
        if (std::ranges::find(structured, name) != std::end(structured))
            return StructuredCommand(name, depth);
        static constexpr std::wstring_view styled[] = {L"text",       L"textbf",     L"textit",   L"textup", L"textmd", L"textnormal", L"operatorname", L"mathrm",    L"mathbf",   L"mathit",
                                                       L"mathnormal", L"boldsymbol", L"bm",       L"mathbb",       L"mathcal",   L"mathfrak", L"mathsf",
                                                       L"mathtt",     L"mathord",    L"mathop",   L"mathbin",      L"mathrel",   L"mathopen", L"mathclose",
                                                       L"mathpunct",  L"mathinner",  L"overline", L"underline",    L"bar",       L"hat",      L"widehat",
                                                       L"vec",        L"dot",        L"ddot",     L"tilde",        L"widetilde", L"check", L"widecheck", L"breve", L"acute", L"grave", L"mathring", L"underbar", L"utilde", L"overbrace", L"underbrace", L"overgroup", L"undergroup", L"left", L"boxed", L"cancel", L"bcancel", L"xcancel", L"sout", L"phase", L"angl"};
        if (std::ranges::find(styled, name) != std::end(styled))
            return StyledCommand(name, depth);
        return SymbolCommand(name);
    }
    MathNode Atom(unsigned depth)
    {
        if (depth > MathMaximumGroupDepth)
            Reject(MathIssue::Capacity);
        Space();
        if (at == source.size())
            Reject(MathIssue::MissingRequiredArgument);
        const auto c = source[at++];
        if (c == L'{')
            return Row(depth + 1, true, false);
        if (c == L'\\')
            return Command(depth);
        if (c == L'&')
            Reject(MathIssue::CellSeparatorOutsideEnvironment);
        if (std::wstring_view(L"}^_#$").find(c) != source.npos)
            Reject();
        if (c == L'~')
            return Make(MathKind::Space, L" ");
        if (c == L'-' || c == L'*')
            return Make(MathKind::Text, std::wstring(1u, c == L'-' ? L'−' : L'∗'));
        std::wstring value(1, c);
        if (c >= 0xD800 && c <= 0xDBFF && at < source.size())
            value += source[at++];
        return Make(MathKind::Text, std::move(value));
    }
    MathNode Row(unsigned depth, bool grouped, bool cell, bool right = false, bool indexClose = false, bool stopBrace = false, bool infixAllowed = true)
    {
        std::vector<MathNode> children(0u);
        while (at < source.size())
        {
            Space();
            if (at == source.size())
                break;
            if (indexClose && source[at] == L']')
            {
                ++at;
                return Make(MathKind::Row, L"", std::move(children));
            }
            if (right && source.substr(at, 6) == L"\\right" && (at + 6 == source.size() || ! Letter(source[at + 6])))
                break;
            if (cell && (source[at] == L'&' || source.substr(at, 2) == L"\\\\" || source.substr(at, 5) == L"\\end{"))
                break;
            if (stopBrace && source[at] == L'}')
                break;
            if (source[at] == L'}')
            {
                if (! grouped)
                    Reject();
                ++at;
                return Make(MathKind::Row, L"", std::move(children));
            }
            if (source[at] == L'\\')
            {
                const auto first = at + 1;
                auto end         = first;
                while (end < source.size() && Letter(source[end]))
                    ++end;
                const auto declaration = source.substr(first, end - first);
                if (declaration == L"over" || declaration == L"atop" || declaration == L"choose" || declaration == L"brace" || declaration == L"brack")
                {
                    if (! infixAllowed)
                        Reject(MathIssue::UnsupportedCommand);
                    at = end;
                    auto numerator = Make(MathKind::Row, L"", std::move(children));
                    auto denominator = Row(depth, grouped, cell, right, indexClose, stopBrace, false);
                    if (declaration == L"over" || declaration == L"atop")
                        return Make(MathKind::Fraction, declaration == L"over" ? L"frac" : L"atop", {std::move(numerator), std::move(denominator)});
                    const auto environment = declaration == L"choose" ? L"pmatrix" : declaration == L"brace" ? L"Bmatrix" :
                                             declaration == L"brack" ? L"bmatrix" : L"matrix";
                    return Make(MathKind::Matrix, environment, {std::move(numerator), std::move(denominator)}, 1u);
                }
                if (declaration == L"tag")
                {
                    if (depth != 0u || equationTag)
                        Reject();
                    at = end;
                    starredTag = at < source.size() && source[at] == L'*';
                    if (starredTag)
                        ++at;
                    Space();
                    if (at == source.size())
                        Reject(MathIssue::MissingRequiredArgument);
                    if (source[at] == L'{')
                    {
                        // This profile has no nested math inside text labels;
                        // escaped dollars are literal text, not delimiters.
                        auto text = GroupText(true, true);
                        auto label = Make(MathKind::Text, std::move(text));
                        label.italic = false;
                        equationTag.emplace(std::move(label));
                    }
                    else
                    {
                        auto label = Atom(depth + 1);
                        label.italic = false;
                        equationTag.emplace(std::move(label));
                    }
                    continue;
                }
                if (declaration == L"color")
                {
                    if (depth >= MathMaximumGroupDepth)
                        Reject(MathIssue::Capacity);
                    at = end;
                    const auto color = Detail::ParseNativeColor(GroupText());
                    if (! color)
                        Reject(MathIssue::UnsupportedCommand);
                    auto label = std::wstring(L"textcolor:");
                    label += Detail::EncodeNativeColor(color.value());
                    AppendColored(children, label, Row(depth + 1u, grouped, cell, right, indexClose, stopBrace, infixAllowed));
                    return Make(MathKind::Row, L"", std::move(children));
                }
                if (declaration == L"displaystyle" || declaration == L"textstyle" || declaration == L"scriptstyle" || declaration == L"scriptscriptstyle" ||
                    declaration == L"tiny" || declaration == L"sixptsize" || declaration == L"scriptsize" || declaration == L"footnotesize" ||
                    declaration == L"small" || declaration == L"normalsize" || declaration == L"large" || declaration == L"Large" ||
                    declaration == L"LARGE" || declaration == L"huge" || declaration == L"Huge")
                {
                    if (depth >= MathMaximumGroupDepth)
                        Reject(MathIssue::Capacity);
                    at = end;
                    children.push_back(Make(MathKind::Style, std::wstring(declaration), {Row(depth + 1, grouped, cell, right, indexClose, stopBrace, infixAllowed)}));
                    return Make(MathKind::Row, L"", std::move(children));
                }
            }
            auto atom = Atom(depth);
            Space();
            MathNode sub, super;
            bool hasSub{}, hasSuper{}, hasModifier{};
            while (at < source.size())
            {
                if (source[at] == L'\'')
                {
                    if (hasSuper)
                        Reject();
                    std::wstring primes(0u, L' ');
                    while (at < source.size() && source[at] == L'\'')
                    {
                        primes.push_back(L'′');
                        ++at;
                    }
                    super    = Make(MathKind::Text, std::move(primes));
                    hasSuper = true;
                    Space();
                    continue;
                }
                if (source[at] == L'\\')
                {
                    const auto first = at + 1;
                    auto end         = first;
                    while (end < source.size() && Letter(source[end]))
                        ++end;
                    const auto modifier = source.substr(first, end - first);
                    if (modifier == L"limits" || modifier == L"nolimits")
                    {
                        if (hasModifier || atom.atomClass != MathAtomClass::Operator)
                            Reject();
                        atom.limits = modifier == L"limits" ? MathLimits::AboveBelow : MathLimits::Side;
                        hasModifier = true;
                        at          = end;
                        Space();
                        continue;
                    }
                }
                if (source[at] != L'^' && source[at] != L'_')
                    break;
                const bool upper = source[at++] == L'^';
                if ((upper && hasSuper) || (! upper && hasSub))
                    Reject(upper ? MathIssue::DuplicateSuperscript : MathIssue::DuplicateSubscript);
                Space();
                if (at == source.size())
                    Reject(MathIssue::MissingScriptArgument);
                auto argument = Atom(depth + 1);
                if (upper)
                {
                    super    = std::move(argument);
                    hasSuper = true;
                }
                else
                {
                    sub    = std::move(argument);
                    hasSub = true;
                }
                Space();
            }
            if (hasSub || hasSuper)
                atom = Make(MathKind::Scripts, L"", {std::move(atom), std::move(sub), std::move(super)});
            if (! hasSub && ! hasSuper && atom.kind == MathKind::Style && atom.text.starts_with(L"textcolor:") && atom.children.size() == 1u &&
                atom.children[0].kind == MathKind::Row)
            {
                const auto label = std::wstring(atom.text);
                AppendColored(children, label, std::move(atom.children[0]));
                continue;
            }
            if (atom.kind == MathKind::Text && atom.atomClass == MathAtomClass::Ordinary && ! children.empty() && children.back().kind == MathKind::Text &&
                children.back().atomClass == MathAtomClass::Ordinary && children.back().italic == atom.italic)
                children.back().text.append(atom.text);
            else
                children.push_back(std::move(atom));
        }
        if (grouped || indexClose)
            Reject(indexClose ? MathIssue::MissingRootIndexClose : MathIssue::MissingGroupClose);
        return Make(MathKind::Row, L"", std::move(children));
    }

public:
    explicit Parser(std::wstring_view value) : source(value), matrixRowWidths(0u)
    {
    }
    MathNode Parse()
    {
        auto body = Row(0, false, false);
        if (! equationTag)
            return body;
        auto tag = std::move(equationTag.value());
        if (! starredTag)
        {
            std::vector<MathNode> children(0u);
            auto left = Make(MathKind::Text, L"(");
            left.italic = false;
            children.push_back(std::move(left));
            children.push_back(std::move(tag));
            auto right = Make(MathKind::Text, L")");
            right.italic = false;
            children.push_back(std::move(right));
            tag = Make(MathKind::Row, L"", std::move(children));
        }
        return Make(MathKind::Style, L"tag", {std::move(body), std::move(tag)});
    }
};
} // namespace
MathExpression ParseMath(std::wstring_view source)
{
    if (source.size() > MathMaximumSourceUnits)
        return {{}, L"Equation source exceeds 16,384 UTF-16 units. Source is preserved.", MathIssue::Capacity};
    if (source.empty() || ! IsValidUtf16(source) ||
        std::ranges::any_of(source, [](wchar_t c) { return (c < L' ' && ! iswspace(c)) || (c >= 0x7f && c <= 0x9f); }))
        return {{}, L"Equation source is empty or invalid. Source is preserved.", MathIssue::InvalidSource};
    try
    {
        auto root        = Parser(source).Parse();
        const auto count = [](auto&& self, const MathNode& node, unsigned depth) -> std::size_t
        {
            if (depth > MathMaximumNodeDepth)
                throw std::invalid_argument("Equation nesting capacity");
            std::size_t result = 1;
            for (const auto& child : node.children)
                result += self(self, child, depth + 1);
            return result;
        };
        if (count(count, root, 0) > MathMaximumNodes)
            throw std::invalid_argument("Equation node capacity");
        return {std::move(root), L""};
    }
    catch (const MathParseFailure& failure)
    {
        return {{}, L"Unsupported or malformed native equation. Source is preserved.", failure.issue};
    }
    catch (const std::invalid_argument&)
    {
        return {{}, L"Unsupported or malformed native equation. Source is preserved.", MathIssue::Capacity};
    }
}
} // namespace DxUi::RichText
