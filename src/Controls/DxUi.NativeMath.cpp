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
        while (at < source.size() && iswspace(source[at]))
            ++at;
    }
    [[noreturn]] static void Reject()
    {
        throw std::invalid_argument("Unsupported or malformed native equation");
    }
    MathNode Make(MathKind kind,
                  std::wstring&& text              = std::wstring(0u, L' '),
                  std::vector<MathNode>&& children = std::vector<MathNode>(0u),
                  unsigned columns                 = 0)
    {
        if (++nodes > 512)
            Reject();
        return {kind, std::move(text), std::move(children), columns};
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
    std::wstring GroupText()
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
                value += c;
            }
            else if (c == L'}')
            {
                if (--depth == 0)
                    return std::wstring(value);
                value += c;
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
        unsigned columns{}, rowColumns{}, rows = 1;
        while (at < source.size())
        {
            cells.push_back(Row(depth + 1, false, true));
            if (++rowColumns > 16 || cells.size() > 128)
                Reject();
            if (source.substr(at, 5) == L"\\end{")
            {
                at += 4;
                if (GroupText() != environment)
                    Reject();
                if (! columns)
                    columns = rowColumns;
                if (columns != rowColumns)
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
            if (columns != rowColumns || ++rows > 16)
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
            if (name == L"vert")
                return L'|';
            if (name == L"Vert")
                return L'‖';
            Reject();
        }
        const auto value = source[at++];
        if (std::wstring_view(L"()[]{}|.").find(value) == source.npos)
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
                return Make(MathKind::Fraction, L"", {Atom(depth + 1), Atom(depth + 1)});
            if (name == L"sqrt")
            {
                Space();
                MathNode index;
                if (at < source.size() && source[at] == L'[')
                {
                    const auto end = source.find(L']', ++at);
                    if (end == source.npos)
                        Reject();
                    auto parsed = ParseMath(source.substr(at, end - at));
                    if (! parsed)
                        Reject();
                    index = std::move(parsed.root);
                    at    = end + 1;
                }
                return Make(MathKind::Root, L"", {Atom(depth + 1), std::move(index)});
            }
            if (name == L"begin")
                return Matrix(GroupText(), depth);
            if (name == L"text" || name == L"operatorname")
                return Make(MathKind::Text, GroupText());
            if (name == L"mathrm" || name == L"mathbf" || name == L"mathit" || name == L"mathbb" || name == L"mathcal")
            {
                auto body = Atom(depth + 1);
                if (name == L"mathbb" || name == L"mathcal")
                    Alphabet(body, name == L"mathcal");
                return Make(MathKind::Style, std::wstring(name), {std::move(body)});
            }
            if (name == L"overline" || name == L"underline" || name == L"bar" || name == L"hat" || name == L"widehat" || name == L"vec" || name == L"dot" ||
                name == L"ddot")
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
            if (name == L"big" || name == L"Big" || name == L"bigg" || name == L"Bigg")
            {
                const auto delimiter = Delimiter();
                return Make(delimiter == L'.' ? MathKind::Space : MathKind::Text, delimiter == L'.' ? L"" : std::wstring(1, delimiter));
            }
            if (name == L"limits" || name == L"nolimits")
                return Make(MathKind::Space);
            if (name == L"quad" || name == L"qquad")
                return Make(MathKind::Space, name == L"quad" ? L"  " : L"    ");
            constexpr std::pair<std::wstring_view, std::wstring_view> symbols[]{{L"alpha", L"α"},
                                                                                {L"beta", L"β"},
                                                                                {L"gamma", L"γ"},
                                                                                {L"delta", L"δ"},
                                                                                {L"epsilon", L"ε"},
                                                                                {L"varepsilon", L"ϵ"},
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
                                                                                {L"phi", L"φ"},
                                                                                {L"varphi", L"ϕ"},
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
                                                                                {L"Phi", L"Φ"},
                                                                                {L"Psi", L"Ψ"},
                                                                                {L"Omega", L"Ω"},
                                                                                {L"sum", L"∑"},
                                                                                {L"prod", L"∏"},
                                                                                {L"int", L"∫"},
                                                                                {L"iint", L"∬"},
                                                                                {L"iiint", L"∭"},
                                                                                {L"oint", L"∮"},
                                                                                {L"infty", L"∞"},
                                                                                {L"times", L"×"},
                                                                                {L"cdot", L"·"},
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
                                                                                {L"sin", L"sin"},
                                                                                {L"cos", L"cos"},
                                                                                {L"tan", L"tan"},
                                                                                {L"cot", L"cot"},
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
                    return Make(MathKind::Text, std::wstring(value));
            if (name.empty() && at < source.size())
            {
                const auto escaped = source[at++];
                if (std::wstring_view(L",;:! ").find(escaped) != source.npos)
                    return Make(MathKind::Space, escaped == L'!' ? L"" : L" ");
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
    MathNode Row(unsigned depth, bool grouped, bool cell, bool right = false)
    {
        std::vector<MathNode> children(0u);
        while (at < source.size())
        {
            Space();
            if (at == source.size())
                break;
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
            auto atom = Atom(depth);
            Space();
            MathNode sub, super;
            bool hasSub{}, hasSuper{};
            while (at < source.size() && (source[at] == L'^' || source[at] == L'_'))
            {
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
            if (atom.kind == MathKind::Text && ! children.empty() && children.back().kind == MathKind::Text)
                children.back().text.append(atom.text);
            else
                children.push_back(std::move(atom));
        }
        if (grouped)
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
    if (source.empty() || source.size() > 4096 || ! IsValidUtf16(source))
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
        return {root, L""};
    }
    catch (const std::invalid_argument&)
    {
        return {{}, L"Unsupported or malformed native equation. Source is preserved."};
    }
}
} // namespace DxUi::RichText
