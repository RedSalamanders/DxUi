#include <DxUi/RichDocument.h>
#include <DxUi/RichMath.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include <stdexcept>

namespace DxUi::RichText
{
namespace
{
class Parser
{
    std::wstring_view source;
    std::size_t at{}, nodes{};
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
    [[noreturn]] static void Reject()
    {
        throw std::invalid_argument("Unsupported or malformed native equation");
    }
    static MathAtomClass Classify(std::wstring_view value) noexcept
    {
        using enum MathAtomClass;
        if (value.size() != 1u)
            return Ordinary;
        const auto c = value[0];
        if (std::wstring_view(L"+-−*×÷⋅±∓∪∩∧∨").find(c) != value.npos)
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
        if (++nodes > 512)
            Reject();
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
    static void Alphabet(MathNode& node, bool script)
    {
        constexpr std::u32string_view blackboard   = U"𝔸𝔹ℂ𝔻𝔼𝔽𝔾ℍ𝕀𝕁𝕂𝕃𝕄ℕ𝕆ℙℚℝ𝕊𝕋𝕌𝕍𝕎𝕏𝕐ℤ";
        constexpr std::u32string_view calligraphic = U"𝒜ℬ𝒞𝒟ℰℱ𝒢ℋℐ𝒥𝒦ℒℳ𝒩𝒪𝒫𝒬ℛ𝒮𝒯𝒰𝒱𝒲𝒳𝒴𝒵";
        constexpr std::u32string_view lowerScript  = U"𝒶𝒷𝒸𝒹ℯ𝒻ℊ𝒽𝒾𝒿𝓀𝓁𝓂𝓃ℴ𝓅𝓆𝓇𝓈𝓉𝓊𝓋𝓌𝓍𝓎𝓏";
        if (node.kind == MathKind::Text)
        {
            std::wstring text(0u, L' ');
            for (wchar_t unit : node.text)
            {
                char32_t scalar = unit;
                if (unit >= L'A' && unit <= L'Z')
                    scalar = (script ? calligraphic : blackboard)[unit - L'A'];
                else if (unit >= L'a' && unit <= L'z')
                    scalar = script ? lowerScript[unit - L'a'] : 0x1D552u + unit - L'a';
                else if (! script && unit >= L'0' && unit <= L'9')
                    scalar = 0x1D7D8u + unit - L'0';
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
            Alphabet(child, script);
    }
    std::wstring GroupText(bool textMode = false)
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
                if (++depth > 24)
                    Reject();
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
    MathNode Matrix(std::wstring environment, unsigned depth)
    {
        constexpr std::array names{L"matrix", L"pmatrix", L"bmatrix", L"Bmatrix", L"vmatrix", L"Vmatrix", L"smallmatrix", L"cases", L"aligned", L"gathered"};
        if (std::ranges::find(names, environment) == names.end())
            Reject();
        std::vector<MathNode> cells(0u);
        unsigned columns{}, rowColumns{}, rows{};
        while (at < source.size())
        {
            Space();
            if (! rowColumns && ! cells.empty() && source.substr(at, 5) == L"\\end{")
            {
                if ((environment == L"gathered" && columns != 1u) || (environment == L"cases" && columns > 2u))
                    Reject();
                at += 4;
                if (GroupText() != environment)
                    Reject();
                return Make(MathKind::Matrix, std::move(environment), std::move(cells), columns);
            }
            if (! rowColumns && ++rows > 16u)
                Reject();
            if (rowColumns == 16u || cells.size() == 128u)
                Reject();
            cells.push_back(Row(depth + 1, false, true));
            ++rowColumns;
            if (source.substr(at, 5) == L"\\end{")
            {
                at += 4;
                if (GroupText() != environment)
                    Reject();
                if (! columns)
                    columns = rowColumns;
                if (columns != rowColumns || (environment == L"gathered" && columns != 1u) || (environment == L"cases" && columns > 2u))
                    Reject();
                return Make(MathKind::Matrix, std::move(environment), std::move(cells), columns);
            }
            if (at < source.size() && source[at] == L'&')
            {
                ++at;
                continue;
            }
            if (source.substr(at, 2) != L"\\\\")
                Reject();
            at += 2;
            if (! columns)
                columns = rowColumns;
            if (columns != rowColumns)
                Reject();
            rowColumns = 0;
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
            if (source[at] == L'{' || source[at] == L'}' || source[at] == L'|')
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
            if (name == L"lfloor")
                return L'⌊';
            if (name == L"rfloor")
                return L'⌋';
            if (name == L"lceil")
                return L'⌈';
            if (name == L"rceil")
                return L'⌉';
            Reject();
        }
        const auto value = source[at++];
        if (std::wstring_view(L"()[]{}|.⌊⌋⌈⌉⟨⟩‖").find(value) == source.npos)
            Reject();
        return value;
    }
    MathNode Atom(unsigned depth)
    {
        if (depth > 24)
            Reject();
        Space();
        if (at == source.size())
            Reject();
        const auto c = source[at++];
        if (c == L'{')
            return Row(depth + 1, true, false);
        if (c == L'\\')
        {
            const auto first = at;
            while (at < source.size() && Letter(source[at]))
                ++at;
            const auto name = source.substr(first, at - first);
            if (name == L"frac" || name == L"dfrac" || name == L"tfrac")
                return Make(MathKind::Fraction, std::wstring(name), {Atom(depth + 1), Atom(depth + 1)});
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
            if (name == L"begin")
                return Matrix(GroupText(), depth);
            if (name == L"text" || name == L"operatorname")
            {
                const bool starred = name == L"operatorname" && at < source.size() && source[at] == L'*';
                if (starred)
                    ++at;
                auto result          = Make(MathKind::Text, GroupText(true));
                result.italic        = false;
                result.atomClass     = name == L"operatorname" ? MathAtomClass::Operator : MathAtomClass::Ordinary;
                result.displayLimits = starred;
                return result;
            }
            if (name == L"mathrm" || name == L"mathbf" || name == L"mathit" || name == L"mathbb" || name == L"mathcal" || name == L"mathord" ||
                name == L"mathop" || name == L"mathbin" || name == L"mathrel" || name == L"mathopen" || name == L"mathclose" || name == L"mathpunct" ||
                name == L"mathinner")
            {
                auto body = Atom(depth + 1);
                if (name == L"mathbb" || name == L"mathcal")
                    Alphabet(body, name == L"mathcal");
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
            if (name == L"overline" || name == L"underline" || name == L"bar" || name == L"hat" || name == L"widehat" || name == L"vec" || name == L"dot" ||
                name == L"ddot" || name == L"tilde" || name == L"widetilde")
                return Make(MathKind::Accent, std::wstring(name), {Atom(depth + 1)});
            if (name == L"left")
            {
                const auto left = Delimiter();
                auto body       = Row(depth + 1, false, false, true);
                if (source.substr(at, 6) != L"\\right")
                    Reject();
                at += 6;
                const auto right = Delimiter();
                return Make(MathKind::Delimited, std::wstring{left, right}, {std::move(body)});
            }
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
                                                                                {L"lambda", L"λ"},
                                                                                {L"mu", L"μ"},
                                                                                {L"nu", L"ν"},
                                                                                {L"xi", L"ξ"},
                                                                                {L"pi", L"π"},
                                                                                {L"rho", L"ρ"},
                                                                                {L"sigma", L"σ"},
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
                if (std::wstring_view(L"{}%_$&#\\").find(escaped) != source.npos)
                    return Make(MathKind::Text, std::wstring(1, escaped));
            }
            Reject();
        }
        if (std::wstring_view(L"}^_&#$").find(c) != source.npos)
            Reject();
        std::wstring value(1, c);
        if (c >= 0xD800 && c <= 0xDBFF && at < source.size())
            value += source[at++];
        return Make(MathKind::Text, std::move(value));
    }
    MathNode Row(unsigned depth, bool grouped, bool cell, bool right = false, bool indexClose = false)
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
                if (declaration == L"displaystyle" || declaration == L"textstyle" || declaration == L"scriptstyle" || declaration == L"scriptscriptstyle")
                {
                    if (depth >= 24u)
                        Reject();
                    at = end;
                    children.push_back(Make(MathKind::Style, std::wstring(declaration), {Row(depth + 1, grouped, cell, right, indexClose)}));
                    return Make(MathKind::Row, L"", std::move(children));
                }
            }
            auto atom = Atom(depth);
            Space();
            MathNode sub, super;
            bool hasSub{}, hasSuper{}, hasModifier{};
            while (at < source.size())
            {
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
                    Reject();
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
            if (atom.kind == MathKind::Text && atom.atomClass == MathAtomClass::Ordinary && ! children.empty() && children.back().kind == MathKind::Text &&
                children.back().atomClass == MathAtomClass::Ordinary && children.back().italic == atom.italic)
                children.back().text.append(atom.text);
            else
                children.push_back(std::move(atom));
        }
        if (grouped || indexClose)
            Reject();
        return Make(MathKind::Row, L"", std::move(children));
    }

public:
    explicit Parser(std::wstring_view value) : source(value)
    {
    }
    MathNode Parse()
    {
        return Row(0, false, false);
    }
};
} // namespace
MathExpression ParseMath(std::wstring_view source)
{
    if (source.empty() || source.size() > MathMaximumSourceUnits || ! IsValidUtf16(source) ||
        std::ranges::any_of(source, [](wchar_t c) { return (c < L' ' && ! iswspace(c)) || (c >= 0x7f && c <= 0x9f); }))
        return {{}, L"Equation source is empty, invalid or exceeds 4,096 UTF-16 units. Source is preserved."};
    try
    {
        auto root        = Parser(source).Parse();
        const auto count = [](auto&& self, const MathNode& node, unsigned depth) -> std::size_t
        {
            if (depth > 24)
                throw std::invalid_argument("Equation nesting capacity");
            std::size_t result = 1;
            for (const auto& child : node.children)
                result += self(self, child, depth + 1);
            return result;
        };
        if (count(count, root, 0) > 512)
            throw std::invalid_argument("Equation node capacity");
        return {std::move(root), L""};
    }
    catch (const std::invalid_argument&)
    {
        return {{}, L"Unsupported or malformed native equation. Source is preserved."};
    }
}
} // namespace DxUi::RichText
