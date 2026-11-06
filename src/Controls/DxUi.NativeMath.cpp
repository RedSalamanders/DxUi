#include "DxUi.NativeColor.h"
#include "DxUi.NativeMathDimension.h"
#include "DxUi.NativeMathSymbols.h"
#include <DxUi/Configuration.h>
#include <DxUi/RichDocument.h>
#include <DxUi/RichMath.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cwctype>
#include <optional>
#include <stdexcept>

namespace DxUi::RichText
{
#if DXUI_ENABLE_DIAGNOSTICS
namespace Detail
{
thread_local std::uintptr_t testRichMathLowestStackAddress{};
}
#endif
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
    struct RowGap
    {
        std::size_t first{}, length{};
    };
    std::optional<std::vector<RowGap>> matrixRowGaps;
    std::optional<MathNode> equationTag;
    bool starredTag{};
    bool hasRowTags{};
    struct RowTagEntry
    {
        unsigned row{};
        MathNode label;
    };
    struct RowTagContext
    {
        unsigned depth{}, row{};
        std::optional<std::vector<RowTagEntry>> tags;
    };
    std::optional<RowTagContext> rootRowTags;
    RowTagContext* activeRowTags{};
    struct RowTagScope
    {
        Parser& parser;
        RowTagContext* previous;
        RowTagScope(Parser& owner, RowTagContext* current) noexcept : parser(owner), previous(owner.activeRowTags)
        {
            owner.activeRowTags = current;
        }
        RowTagScope(const RowTagScope&)            = delete;
        RowTagScope& operator=(const RowTagScope&) = delete;
        RowTagScope(RowTagScope&&)                 = delete;
        RowTagScope& operator=(RowTagScope&&)      = delete;
        ~RowTagScope() noexcept
        {
            parser.activeRowTags = previous;
        }
    };
    static void ObserveStack() noexcept
    {
#if DXUI_ENABLE_DIAGNOSTICS
        if (Detail::testRichMathLowestStackAddress)
        {
            const char marker{};
            const auto address                     = reinterpret_cast<std::uintptr_t>(&marker);
            Detail::testRichMathLowestStackAddress = std::min(Detail::testRichMathLowestStackAddress, address);
        }
#endif
    }
    std::size_t RowSeparatorLength() const noexcept
    {
        if (source.substr(at, 2u) == L"\\\\")
            return 2u;
        if (source.substr(at, 3u) == L"\\cr" && (at + 3u == source.size() || ! Letter(source[at + 3u])))
            return 3u;
        return 0u;
    }
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
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
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
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        // An explicitly nested alphabet or text family owns its own glyphs;
        // an enclosing alphabet must not rewrite that override afterward.
        if (node.kind == MathKind::Style &&
            (node.text == L"mathrm" || node.text == L"mathit" || node.text == L"mathnormal" || node.text == L"mathbb" || node.text == L"mathcal" ||
             node.text == L"mathscr" || node.text == L"mathfrak" || node.text == L"mathsf" || node.text == L"mathtt" || node.text == L"textrm" ||
             node.text == L"textsf" || node.text == L"texttt" || node.text == L"textnormal" || node.text == L"emph"))
            return;
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
    bool AppendTextAccent(std::wstring& text)
    {
        if (at >= source.size())
            return false;
        const wchar_t command = source[at];
        wchar_t mark{};
        switch (command)
        {
            case L'\'': mark = L'\u0301'; break;
            case L'`': mark = L'\u0300'; break;
            case L'^': mark = L'\u0302'; break;
            case L'"': mark = L'\u0308'; break;
            case L'~': mark = L'\u0303'; break;
            case L'=': mark = L'\u0304'; break;
            case L'.': mark = L'\u0307'; break;
            case L'u': mark = L'\u0306'; break;
            case L'v': mark = L'\u030c'; break;
            case L'H': mark = L'\u030b'; break;
            case L'c': mark = L'\u0327'; break;
            case L'k': mark = L'\u0328'; break;
            case L'r': mark = L'\u030a'; break;
            case L'b': mark = L'\u0331'; break;
            case L'd': mark = L'\u0323'; break;
            default: return false;
        }
        if (Letter(command) && at + 1u < source.size() && Letter(source[at + 1u]))
            return false;
        ++at;
        Space();
        const bool grouped = at < source.size() && source[at] == L'{';
        if (grouped)
            ++at;
        if (at == source.size() || source[at] == L'{' || source[at] == L'}' || source[at] == L'\\' || iswspace(source[at]))
            Reject(MathIssue::MissingRequiredArgument);
        const wchar_t first = source[at++];
        text.push_back(first);
        if (first >= 0xd800u && first <= 0xdbffu)
        {
            if (at == source.size() || source[at] < 0xdc00u || source[at] > 0xdfffu)
                Reject(MathIssue::InvalidSource);
            text.push_back(source[at++]);
        }
        if (grouped && (at == source.size() || source[at++] != L'}'))
            Reject(MathIssue::UnsupportedCommand);
        text.push_back(mark);
        return true;
    }
    std::wstring GroupText(bool textMode = false, bool rejectMathDelimiters = false)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
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
                if (textMode && AppendTextAccent(value))
                    continue;
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
            if (ch == L'|' || ch == L':')
            {
                if (rules.back() != L'.')
                    Reject(MathIssue::UnsupportedCommand);
                rules.back() = ch;
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
        if (rules.find_first_of(L"|:") != std::wstring::npos)
        {
            columns.push_back(L':');
            columns.append(rules);
        }
        return std::wstring(columns);
    }
    bool ConsumeArrayRule(unsigned& row)
    {
        if (at >= source.size() || source[at] != L'\\')
            return false;
        const bool solid  = source.substr(at, 6u) == L"\\hline" && (at + 6u == source.size() || ! Letter(source[at + 6u]));
        const bool dashed = source.substr(at, 10u) == L"\\hdashline" && (at + 10u == source.size() || ! Letter(source[at + 10u]));
        if (! solid && ! dashed)
            return false;
        if ((row & 0x300u) != 0u)
            Reject(MathIssue::UnsupportedCommand);
        row |= solid ? 0x100u : 0x200u;
        at += solid ? 6u : 10u;
        return true;
    }
    void ConsumeRowSpacing(unsigned depth, std::size_t row, std::size_t separatorLength)
    {
        Space();
        if (at == source.size() || source[at] != L'[')
            return;
        if (separatorLength != 2u)
            Reject(MathIssue::UnsupportedCommand);
        const auto first = ++at;
        while (at < source.size() && source[at] != L']')
            ++at;
        if (at == source.size() || ! Detail::ParseMathDimension(source.substr(first, at - first)))
            Reject(MathIssue::UnsupportedCommand);
        if (! matrixRowGaps)
            matrixRowGaps.emplace((MathMaximumGroupDepth + 1u) * MathMaximumRows);
        (*matrixRowGaps)[static_cast<std::size_t>(depth) * MathMaximumRows + row] = {first, at - first};
        ++at;
    }
    void AppendRowSpacings(std::wstring& name, unsigned depth, std::size_t rows)
    {
        if (! matrixRowGaps)
            return;
        const auto offset = static_cast<std::size_t>(depth) * MathMaximumRows;
        bool any{};
        for (std::size_t row = 0u; row < rows; ++row)
            any = any || (*matrixRowGaps)[offset + row].length != 0u;
        if (! any)
            return;
        name.append(L";gaps:");
        for (std::size_t row = 0u; row < rows; ++row)
        {
            if (row)
                name.push_back(L',');
            const auto gap = (*matrixRowGaps)[offset + row];
            if (gap.length)
                name.append(source.substr(gap.first, gap.length));
            else
                name.push_back(L'.');
        }
    }
    static bool IsArrayEnvironment(std::wstring_view name) noexcept
    {
        return name == L"array" || name == L"darray";
    }
    static bool IsAlignatEnvironment(std::wstring_view name) noexcept
    {
        return name == L"alignat" || name == L"alignat*" || name == L"alignedat";
    }
    static bool AllowsRowTags(std::wstring_view name) noexcept
    {
        static constexpr std::wstring_view names[]{L"align", L"align*", L"alignat", L"alignat*", L"gather", L"gather*", L"equation", L"equation*"};
        return std::ranges::find(names, name) != std::end(names);
    }
    struct MatrixHeader
    {
        std::wstring columnSpec = std::wstring(0u, L' ');
        unsigned declaredColumns{};
        wchar_t matrixAlignment{};
    };
    void InitializeMatrix(std::wstring_view environment, MatrixHeader& header)
    {
        static constexpr std::array names{L"matrix",   L"pmatrix",   L"bmatrix",  L"Bmatrix",   L"vmatrix",  L"Vmatrix",  L"smallmatrix", L"cases",
                                          L"rcases",   L"dcases",    L"drcases",  L"aligned",   L"gathered", L"align",    L"align*",      L"split",
                                          L"gather",   L"gather*",   L"equation", L"equation*", L"array",    L"darray",   L"subarray",    L"alignat",
                                          L"alignat*", L"alignedat", L"matrix*",  L"pmatrix*",  L"bmatrix*", L"Bmatrix*", L"vmatrix*",    L"Vmatrix*"};
        if (std::ranges::find(names, environment) == names.end())
            Reject(MathIssue::UnsupportedEnvironment);
        header.matrixAlignment = MatrixAlignment(environment);
        auto& columnSpec       = header.columnSpec;
        auto& declaredColumns  = header.declaredColumns;
        if (IsArrayEnvironment(environment))
        {
            columnSpec.assign(ArraySpecification());
            const auto separator = columnSpec.find(L':');
            declaredColumns      = static_cast<unsigned>(separator == std::wstring::npos ? columnSpec.size() : separator);
        }
        else if (environment == L"subarray")
        {
            const auto alignment = GroupText();
            if (alignment != L"l" && alignment != L"c")
                Reject(MathIssue::UnsupportedCommand);
            columnSpec      = alignment;
            declaredColumns = 1u;
        }
        else if (IsAlignatEnvironment(environment))
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
    }
    bool ConsumeEnvironmentEnd(std::wstring_view environment)
    {
        if (source.substr(at, 5u) != L"\\end{")
            return false;
        at += 4u;
        if (GroupText() != environment)
            Reject(MathIssue::MismatchedEnvironment);
        return true;
    }
    void WrapRowTags(std::vector<MathNode>& cells, unsigned columns, RowTagContext& rowTags)
    {
        if (! rowTags.tags)
            return;
        for (auto& entry : rowTags.tags.value())
        {
            const auto index = static_cast<std::size_t>(entry.row) * columns;
            if (index >= cells.size())
                Reject(MathIssue::Capacity);
            cells[index] = Make(MathKind::Style, L"rowtag", {std::move(cells[index]), std::move(entry.label)});
        }
    }
    MathNode Matrix(std::wstring environment, unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        RowTagContext* rowTags{};
        if (depth == 0u && AllowsRowTags(environment))
        {
            rootRowTags.emplace();
            rootRowTags->depth = depth;
            rowTags            = &rootRowTags.value();
        }
        RowTagScope tagScope(*this, rowTags);
        MatrixHeader header;
        InitializeMatrix(environment, header);
        const auto matrixAlignment = header.matrixAlignment;
        auto& columnSpec           = header.columnSpec;
        const auto declaredColumns = header.declaredColumns;
        std::vector<MathNode> cells(0u);
        if (matrixRowWidths.empty())
            matrixRowWidths.resize((MathMaximumGroupDepth + 1u) * (MathMaximumRows + 1u));
        auto* const rowWidths = matrixRowWidths.data() + static_cast<std::size_t>(depth) * (MathMaximumRows + 1u);
        std::fill_n(rowWidths, MathMaximumRows + 1u, 0u);
        if (matrixRowGaps)
            std::fill_n(matrixRowGaps->data() + static_cast<std::size_t>(depth) * MathMaximumRows, MathMaximumRows, RowGap{});
        unsigned columns{}, rowColumns{}, rows{};
        const auto finish = [&]() -> MathNode
        {
            if (rowColumns)
            {
                rowWidths[rows - 1u] |= rowColumns;
                columns = std::max(columns, rowColumns);
            }
            if (declaredColumns && columns > declaredColumns)
                Reject(MathIssue::Capacity);
            if (declaredColumns)
                columns = declaredColumns;
            const bool gathered = environment == L"gathered" || environment == L"gather" || environment == L"gather*" || environment == L"equation" ||
                                  environment == L"equation*";
            const bool equation = environment == L"equation" || environment == L"equation*";
            if ((gathered && columns != 1u) || (equation && rows != 1u) ||
                ((environment == L"cases" || environment == L"rcases" || environment == L"dcases" || environment == L"drcases") && columns > 2u))
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
            else if (environment == L"alignat*" || environment == L"alignedat")
                environment = L"alignat";
            else if (environment == L"subarray")
                environment = L"subarray:" + columnSpec;
            else if (IsArrayEnvironment(environment))
            {
                environment = environment + L":" + columnSpec;
                bool horizontal{};
                for (unsigned row = 0u; row <= rows; ++row)
                    horizontal = horizontal || (rowWidths[row] & 0x300u) != 0u;
                if (horizontal)
                {
                    if (columnSpec.find(L':') == std::wstring::npos)
                    {
                        environment.push_back(L':');
                        environment.append(columns + 1u, L'.');
                    }
                    environment.push_back(L':');
                    for (unsigned row = 0u; row <= rows; ++row)
                        environment.push_back((rowWidths[row] & 0x100u) != 0u ? L'-' : (rowWidths[row] & 0x200u) != 0u ? L'=' : L'.');
                }
            }
            else if (gathered)
                environment = L"gathered";
            AppendRowSpacings(environment, depth, rows);
            if (cells.size() == static_cast<std::size_t>(rows) * columns)
            {
                if (rowTags)
                    WrapRowTags(cells, columns, *rowTags);
                return Make(MathKind::Matrix, std::move(environment), std::move(cells), columns);
            }
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
            if (rowTags)
                WrapRowTags(rectangular, columns, *rowTags);
            return Make(MathKind::Matrix, std::move(environment), std::move(rectangular), columns);
        };
        while (at < source.size())
        {
            Space();
            if ((IsArrayEnvironment(environment)) && ! rowColumns && ConsumeArrayRule(rowWidths[rows]))
                continue;
            if (! rowColumns && ! cells.empty() && ConsumeEnvironmentEnd(environment))
                return finish();
            if (! rowColumns && ++rows > MathMaximumRows)
                Reject(MathIssue::Capacity);
            if (rowColumns == MathMaximumColumns || cells.size() == MathMaximumMatrixCells)
                Reject(MathIssue::Capacity);
            if (rowTags)
                rowTags->row = rows - 1u;
            cells.push_back(Row(depth + 1, false, true));
            ++rowColumns;
            if (ConsumeEnvironmentEnd(environment))
                return finish();
            if (at < source.size() && source[at] == L'&')
            {
                ++at;
                continue;
            }
            if (! RowSeparatorLength())
                Reject();
            const auto separatorLength = RowSeparatorLength();
            at += separatorLength;
            ConsumeRowSpacing(depth, rows - 1u, separatorLength);
            rowWidths[rows - 1u] |= rowColumns;
            columns    = std::max(columns, rowColumns);
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
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        Space();
        auto lower = Make(MathKind::Row);
        if (at < source.size() && source[at] == L'[')
        {
            ++at;
            lower = Row(depth + 1u, false, false, false, true);
        }
        auto upper      = Atom(depth + 1u);
        auto arrow      = Make(MathKind::Accent, std::wstring(name), {std::move(upper), std::move(lower)});
        arrow.atomClass = MathAtomClass::Relation;
        return arrow;
    }
    std::wstring GeneralFractionDelimiter()
    {
        Space();
        if (at < source.size() && source[at] == L'{')
            return GroupText();
        return std::wstring(1u, Delimiter());
    }
    MathNode GeneralFraction(unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        const auto left      = GeneralFractionDelimiter();
        const auto right     = GeneralFractionDelimiter();
        const auto rule      = GroupText();
        const auto style     = GroupText();
        const auto delimiter = [](std::wstring_view value) -> wchar_t
        {
            if (value.empty() || value == L".")
                return L'.';
            if (value.size() != 1u || std::wstring_view(L"()[]{}|⟨⟩‖⌊⌋⌈⌉/<>\\↑↓↕⇑⇓⇕").find(value[0u]) == std::wstring_view::npos)
                Reject(MathIssue::UnsupportedCommand);
            return value[0u];
        };
        if (! rule.empty() && ! Detail::ParseMathDimension(rule))
            Reject(MathIssue::UnsupportedCommand);
        if (! style.empty() && style != L"0" && style != L"1" && style != L"2" && style != L"3")
            Reject(MathIssue::UnsupportedCommand);
        const auto leftGlyph = delimiter(left), rightGlyph = delimiter(right);
        auto numerator   = Atom(depth + 1u);
        auto denominator = Atom(depth + 1u);
        auto fraction    = Make(MathKind::Fraction, rule.empty() ? std::wstring(L"frac") : L"rule:" + rule, {std::move(numerator), std::move(denominator)});
        if (leftGlyph != L'.' || rightGlyph != L'.')
            fraction = Make(MathKind::Delimited, std::wstring{leftGlyph, rightGlyph}, {std::move(fraction)});
        if (! style.empty())
        {
            const auto name = style == L"0" ? L"displaystyle" : style == L"1" ? L"textstyle" : style == L"2" ? L"scriptstyle" : L"scriptscriptstyle";
            fraction        = Make(MathKind::Style, name, {std::move(fraction)});
        }
        return fraction;
    }
    MathNode Substack(unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        Space();
        if (at == source.size() || source[at++] != L'{')
            Reject(MathIssue::MissingRequiredArgument);
        if (matrixRowGaps)
            std::fill_n(matrixRowGaps->data() + static_cast<std::size_t>(depth) * MathMaximumRows, MathMaximumRows, RowGap{});
        std::vector<MathNode> rows(0u);
        while (true)
        {
            if (rows.size() == MathMaximumRows)
                Reject(MathIssue::Capacity);
            rows.push_back(Row(depth + 1u, false, true, false, false, true));
            if (RowSeparatorLength())
            {
                const auto separatorLength = RowSeparatorLength();
                at += separatorLength;
                ConsumeRowSpacing(depth, rows.size() - 1u, separatorLength);
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
        // TeX substack is a centered script-style subarray. Keep it distinct
        // from gathered, whose cells use display style even in inline math.
        std::wstring name(L"subarray:c");
        AppendRowSpacings(name, depth, rows.size());
        return Make(MathKind::Matrix, std::move(name), std::move(rows), 1u);
    }
    static bool UnaryAccent(std::wstring_view name) noexcept
    {
        static constexpr std::wstring_view names[] = {L"overbrace",
                                                      L"underbrace",
                                                      L"overgroup",
                                                      L"undergroup",
                                                      L"boxed",
                                                      L"cancel",
                                                      L"bcancel",
                                                      L"xcancel",
                                                      L"sout",
                                                      L"phase",
                                                      L"angl",
                                                      L"overline",
                                                      L"underline",
                                                      L"bar",
                                                      L"hat",
                                                      L"widehat",
                                                      L"vec",
                                                      L"dot",
                                                      L"ddot",
                                                      L"tilde",
                                                      L"widetilde",
                                                      L"check",
                                                      L"widecheck",
                                                      L"breve",
                                                      L"acute",
                                                      L"grave",
                                                      L"mathring",
                                                      L"underbar",
                                                      L"utilde",
                                                      L"overleftarrow",
                                                      L"overrightarrow",
                                                      L"overleftrightarrow",
                                                      L"Overrightarrow",
                                                      L"underleftarrow",
                                                      L"underrightarrow",
                                                      L"underleftrightarrow",
                                                      L"overleftharpoon",
                                                      L"overrightharpoon",
                                                      L"overlinesegment",
                                                      L"underlinesegment"};
        return std::ranges::find(names, name) != std::end(names);
    }
    MathNode UnaryAccentCommand(std::wstring_view name, unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        auto result = Make(MathKind::Accent, std::wstring(name), {Atom(depth + 1u)});
        if (name == L"overbrace" || name == L"underbrace" || name == L"overgroup" || name == L"undergroup")
        {
            result.atomClass = MathAtomClass::Operator;
            result.limits    = MathLimits::AboveBelow;
        }
        return result;
    }
    MathNode RootCommand(unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        Space();
        MathNode index;
        if (at < source.size() && source[at] == L'[')
        {
            ++at;
            index = Row(depth + 1u, false, false, false, true);
        }
        return Make(MathKind::Root, L"", {Atom(depth + 1), std::move(index)});
    }
    MathNode FractionCommand(std::wstring_view name, unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (name == L"cfrac")
        {
            Space();
            if (at < source.size() && source[at] == L'[')
                Reject(MathIssue::UnsupportedCommand);
        }
        return Make(MathKind::Fraction, std::wstring(name), {Atom(depth + 1u), Atom(depth + 1u)});
    }
    MathNode BinomialCommand(std::wstring_view name, unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        auto binomial = Make(MathKind::Matrix, L"pmatrix", {Atom(depth + 1), Atom(depth + 1)}, 1u);
        if (name == L"binom")
            return binomial;
        return Make(MathKind::Style, name == L"dbinom" ? L"displaystyle" : L"textstyle", {std::move(binomial)});
    }
    MathNode StackCommand(std::wstring_view name, unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
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
    MathNode NotCommand(unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        auto operand = Atom(depth + 1u);
        if (operand.kind != MathKind::Text || operand.atomClass != MathAtomClass::Relation || operand.text.size() != 1u)
            Reject(MathIssue::UnsupportedCommand);
        static constexpr std::array pairs{std::pair{L'=', L'≠'},
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
    MathNode StructuredCommand(std::wstring_view name, unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (name == L"begin")
            return Matrix(GroupText(), depth);
        Reject(MathIssue::UnsupportedCommand);
    }
    static bool MathStyleCommand(std::wstring_view name) noexcept
    {
        static constexpr std::wstring_view names[] = {L"bold",    L"frak",   L"Bbb",     L"mathrm",  L"mathbf",   L"mathit",    L"mathnormal", L"boldsymbol",
                                                      L"bm",      L"mathbb", L"mathcal", L"mathscr", L"mathfrak", L"mathsf",    L"mathtt",     L"pmb",
                                                      L"mathord", L"mathop", L"mathbin", L"mathrel", L"mathopen", L"mathclose", L"mathpunct",  L"mathinner"};
        return std::ranges::find(names, name) != std::end(names);
    }
    static bool ScriptAlphabet(const MathNode& node) noexcept
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (node.kind == MathKind::Text)
            return ! node.text.empty() && std::ranges::all_of(node.text, [](wchar_t ch) { return ch >= L'A' && ch <= L'Z'; });
        if (node.kind != MathKind::Row)
            return false;
        return std::ranges::all_of(node.children, [](const MathNode& child) { return ScriptAlphabet(child); });
    }
    MathNode FinishMathStyle(std::wstring_view name, MathNode&& body)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (name == L"bold")
            name = L"mathbf";
        else if (name == L"frak")
            name = L"mathfrak";
        else if (name == L"Bbb")
            name = L"mathbb";
        if (name == L"mathscr" && ! ScriptAlphabet(body))
            Reject(MathIssue::UnsupportedCommand);
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
    MathNode MathStyle(std::wstring_view name, unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        auto body = Atom(depth + 1u);
        return FinishMathStyle(name, std::move(body));
    }
    MathNode FinishDelimited(wchar_t left, wchar_t right, MathNode&& body)
    {
        return Make(MathKind::Delimited, std::wstring{left, right}, {std::move(body)});
    }
    MathNode LeftCommand(unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        const auto left = Delimiter();
        auto body       = Row(depth + 1u, false, false, true);
        if (source.substr(at, 6u) != L"\\right")
            Reject(MathIssue::MissingRightDelimiter);
        at += 6u;
        const auto right = Delimiter();
        return FinishDelimited(left, right, std::move(body));
    }
    MathNode StyledCommand(std::wstring_view name)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (name == L"bold")
            name = L"mathbf";
        else if (name == L"frak")
            name = L"mathfrak";
        else if (name == L"Bbb")
            name = L"mathbb";
        if (name == L"text" || name == L"textbf" || name == L"textit" || name == L"textup" || name == L"textmd" || name == L"textnormal" || name == L"textrm" ||
            name == L"textsf" || name == L"texttt" || name == L"emph" || name == L"operatorname")
        {
            const bool starred = name == L"operatorname" && at < source.size() && source[at] == L'*';
            if (starred)
                ++at;
            auto result          = Make(MathKind::Text, GroupText(true));
            result.italic        = false;
            result.atomClass     = name == L"operatorname" ? MathAtomClass::Operator : MathAtomClass::Ordinary;
            result.displayLimits = starred;
            if (name == L"textup" || name == L"textmd" || name == L"textnormal" || name == L"textrm" || name == L"textsf" || name == L"texttt" ||
                name == L"emph")
                return Make(MathKind::Style, name == L"textup" ? std::wstring(L"mathrm") : std::wstring(name), {std::move(result)});
            if (name == L"textbf" || name == L"textit")
                return Make(MathKind::Style, name == L"textbf" ? L"mathbf" : L"mathit", {std::move(result)});
            return result;
        }
        Reject(MathIssue::UnsupportedCommand);
    }
    MathNode SymbolCommand(std::wstring_view name)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
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
                // KaTeX's math-mode ldots is an Inner atom; the same literal
                // Unicode glyph remains ordinary when authored directly.
                if (name == L"ldots")
                    result.atomClass = MathAtomClass::Inner;
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
    MathNode TextColorBody(unsigned depth, bool allowDollar = false, bool* sharedSpace = nullptr)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (depth > MathMaximumGroupDepth)
            Reject(MathIssue::Capacity);
        Space();
        if (at == source.size() || source[at++] != L'{')
            Reject(MathIssue::MissingRequiredArgument);
        std::vector<MathNode> children(0u);
        std::wstring text(0u, L' ');
        std::wstring activeColor(0u, L' ');
        bool localSpace{};
        bool& previousSpace = sharedSpace ? *sharedSpace : localSpace;
        const auto append   = [&](MathNode&& node)
        {
            if (! activeColor.empty())
                node = Make(MathKind::Style, std::wstring(activeColor), {std::move(node)});
            children.push_back(std::move(node));
        };
        const auto flush = [&]
        {
            if (text.empty())
                return;
            auto leaf      = Make(MathKind::Text, std::move(text));
            leaf.italic    = false;
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
            if (unit == L'$' && ! allowDollar)
                Reject(MathIssue::UnsupportedCommand);
            if (unit == L'{')
            {
                --at;
                flush();
                append(TextColorBody(depth + 1u, allowDollar, &previousSpace));
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
                if (AppendTextAccent(text))
                {
                    previousSpace = false;
                    continue;
                }
                if (! Letter(source[at]))
                {
                    if (std::wstring_view(L"{}%_$&#\\ ").find(source[at]) == std::wstring_view::npos)
                        Reject(MathIssue::UnsupportedCommand);
                    const auto escaped = source[at++];
                    text.push_back(escaped);
                    previousSpace = escaped == L' ';
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
                {
                    const bool transparent = name == L"textcolor";
                    append(ColorCommand(name, depth, true, transparent ? &previousSpace : nullptr, allowDollar));
                    if (! transparent)
                        previousSpace = false;
                }
                else if (name == L"textbf" || name == L"textit" || name == L"textrm" || name == L"textsf" || name == L"texttt" || name == L"textup" ||
                         name == L"textmd" || name == L"textnormal" || name == L"emph" || name == L"pmb")
                    append(Make(MathKind::Style,
                                name == L"textbf"   ? std::wstring(L"mathbf")
                                : name == L"textit" ? std::wstring(L"mathit")
                                : name == L"textup" ? std::wstring(L"mathrm")
                                                    : std::wstring(name),
                                {TextColorBody(depth + 1u, allowDollar, &previousSpace)}));
                else
                    Reject(MathIssue::UnsupportedCommand);
                continue;
            }
            if (iswspace(unit))
            {
                if (! previousSpace)
                    text.push_back(L' ');
                previousSpace = true;
            }
            else
            {
                text.push_back(unit);
                previousSpace = false;
            }
        }
        Reject(MathIssue::MissingGroupClose);
    }
    MathNode ColorCommand(std::wstring_view name, unsigned depth, bool textMode = false, bool* sharedSpace = nullptr, bool allowDollar = false)
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
            return Make(MathKind::Style, std::move(label), {textMode ? TextColorBody(depth + 1u, allowDollar, sharedSpace) : Atom(depth + 1u)});
        return Make(MathKind::Accent, std::move(label), {TextColorBody(depth + 1u, allowDollar, sharedSpace)});
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
    MathNode TextStyleCommand(std::wstring_view name, unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        auto body = TextColorBody(depth + 1u, true);
        if (name == L"text")
            return body;
        const auto style = name == L"textbf"   ? std::wstring_view(L"mathbf")
                           : name == L"textit" ? std::wstring_view(L"mathit")
                           : name == L"textup" ? std::wstring_view(L"mathrm")
                                               : name;
        return Make(MathKind::Style, std::wstring(style), {std::move(body)});
    }
    static bool IsExtensibleArrowCommand(std::wstring_view value) noexcept
    {
        static constexpr std::wstring_view names[] = {L"xrightarrow",
                                                      L"xleftarrow",
                                                      L"xleftrightarrow",
                                                      L"xRightarrow",
                                                      L"xLeftarrow",
                                                      L"xLeftrightarrow",
                                                      L"xmapsto",
                                                      L"xlongequal",
                                                      L"xhookleftarrow",
                                                      L"xhookrightarrow",
                                                      L"xleftharpoonup",
                                                      L"xleftharpoondown",
                                                      L"xrightharpoonup",
                                                      L"xrightharpoondown",
                                                      L"xleftrightharpoons",
                                                      L"xrightleftharpoons",
                                                      L"xtwoheadleftarrow",
                                                      L"xtwoheadrightarrow",
                                                      L"xtofrom"};
        return std::ranges::find(names, value) != std::end(names);
    }
    static bool IsTextStyleCommand(std::wstring_view value) noexcept
    {
        static constexpr std::wstring_view names[] = {
            L"text", L"textbf", L"textit", L"textup", L"textmd", L"textnormal", L"textrm", L"textsf", L"texttt", L"emph"};
        return std::ranges::find(names, value) != std::end(names);
    }
    static bool IsColorCommand(std::wstring_view name) noexcept
    {
        return name == L"textcolor" || name == L"colorbox" || name == L"fcolorbox";
    }
    static bool IsFractionCommand(std::wstring_view name) noexcept
    {
        static constexpr std::wstring_view names[]{L"frac", L"dfrac", L"tfrac", L"cfrac"};
        return std::ranges::find(names, name) != std::end(names);
    }
    MathNode BeginCommand(unsigned depth)
    {
        return Matrix(GroupText(), depth);
    }
    static bool IsBinomialCommand(std::wstring_view name) noexcept
    {
        static constexpr std::wstring_view names[]{L"binom", L"dbinom", L"tbinom"};
        return std::ranges::find(names, name) != std::end(names);
    }
    static bool IsStackCommand(std::wstring_view name) noexcept
    {
        static constexpr std::wstring_view names[]{L"overset", L"underset", L"stackrel"};
        return std::ranges::find(names, name) != std::end(names);
    }
    MathNode Command(unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        const auto first = at;
        while (at < source.size() && Letter(source[at]))
            ++at;
        const auto name = source.substr(first, at - first);
        if (IsColorCommand(name))
            return ColorCommand(name, depth);
        if (IsExtensibleArrowCommand(name))
            return ExtensibleArrowCommand(name, depth);
        if (IsTextStyleCommand(name))
            return TextStyleCommand(name, depth);
        if (MathStyleCommand(name))
            return MathStyle(name, depth);
        if (UnaryAccent(name))
            return UnaryAccentCommand(name, depth);
        if (name == L"begin")
            return BeginCommand(depth);
        if (name == L"not")
            return NotCommand(depth);
        if (name == L"left")
            return LeftCommand(depth);
        if (name == L"sqrt")
            return RootCommand(depth);
        if (name == L"substack")
            return Substack(depth);
        if (name == L"genfrac")
            return GeneralFraction(depth);
        if (IsBinomialCommand(name))
            return BinomialCommand(name, depth);
        if (IsStackCommand(name))
            return StackCommand(name, depth);
        if (IsFractionCommand(name))
            return FractionCommand(name, depth);
        static constexpr std::wstring_view structured[] = {
            L"not", L"frac", L"dfrac", L"tfrac", L"cfrac", L"binom", L"dbinom", L"tbinom", L"overset", L"underset", L"stackrel", L"sqrt"};
        if (std::ranges::find(structured, name) != std::end(structured))
            return StructuredCommand(name, depth);
        static constexpr std::wstring_view styled[] = {
            L"text",     L"textbf",       L"textit",    L"textup",     L"textmd",    L"textnormal", L"textrm",  L"textsf", L"texttt",     L"emph",
            L"pmb",      L"operatorname", L"bold",      L"frak",       L"Bbb",       L"mathrm",     L"mathbf",  L"mathit", L"mathnormal", L"boldsymbol",
            L"bm",       L"mathbb",       L"mathcal",   L"mathfrak",   L"mathsf",    L"mathtt",     L"mathord", L"mathop", L"mathbin",    L"mathrel",
            L"mathopen", L"mathclose",    L"mathpunct", L"mathinner",  L"overline",  L"underline",  L"bar",     L"hat",    L"widehat",    L"vec",
            L"dot",      L"ddot",         L"tilde",     L"widetilde",  L"check",     L"widecheck",  L"breve",   L"acute",  L"grave",      L"mathring",
            L"underbar", L"utilde",       L"overbrace", L"underbrace", L"overgroup", L"undergroup", L"left",    L"boxed",  L"cancel",     L"bcancel",
            L"xcancel",  L"sout",         L"phase",     L"angl"};
        if (std::ranges::find(styled, name) != std::end(styled))
            return StyledCommand(name);
        return SymbolCommand(name);
    }
    MathNode Atom(unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (depth > MathMaximumGroupDepth)
            Reject(MathIssue::Capacity);
        Space();
        if (at == source.size())
            Reject(MathIssue::MissingRequiredArgument);
        const auto c = source[at++];
        if (c == L'{')
            return Row(depth + 1, true, false);
        if (c == L'\\')
        {
            // Environments are commonly nested at the admitted depth limit.
            // Dispatch their exact command directly, avoiding the much larger
            // general command frame on every container level.
            if (source.substr(at, 5u) == L"begin" && (at + 5u == source.size() || ! Letter(source[at + 5u])))
            {
                at += 5u;
                return BeginCommand(depth);
            }
            return Command(depth);
        }
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
    bool ApplyScripts(MathNode& atom, unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
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
        return hasSub || hasSuper;
    }
    std::wstring RuleDimensionSource()
    {
        Space();
        if (at < source.size() && source[at] == L'{')
        {
            const auto dimension = GroupText();
            if (! Detail::ParseMathDimension(dimension))
                Reject(MathIssue::UnsupportedCommand);
            return std::wstring(dimension);
        }
        const auto first = at;
        while (at < source.size() && ((source[at] >= L'0' && source[at] <= L'9') || source[at] == L'.'))
            ++at;
        if (at + 2u <= source.size() && (source.substr(at, 2u) == L"pt" || source.substr(at, 2u) == L"em" || source.substr(at, 2u) == L"ex"))
            at += 2u;
        const auto dimension = source.substr(first, at - first);
        if (! Detail::ParseMathDimension(dimension))
            Reject(MathIssue::UnsupportedCommand);
        return std::wstring(dimension);
    }
    MathNode FinishInfix(std::wstring_view declaration, const std::wstring& dimension, MathNode&& numerator, MathNode&& denominator)
    {
        if (declaration == L"over" || declaration == L"atop" || declaration == L"above")
        {
            const auto kind = declaration == L"over" ? std::wstring(L"frac") : declaration == L"atop" ? std::wstring(L"atop") : L"rule:" + dimension;
            return Make(MathKind::Fraction, std::wstring(kind), {std::move(numerator), std::move(denominator)});
        }
        const auto environment = declaration == L"choose" ? L"pmatrix" : declaration == L"brace" ? L"Bmatrix" : L"bmatrix";
        return Make(MathKind::Matrix, environment, {std::move(numerator), std::move(denominator)}, 1u);
    }
    MathNode FinishRow(std::vector<MathNode>& children)
    {
        return Make(MathKind::Row, L"", std::move(children));
    }
    MathNode InfixRow(
        std::vector<MathNode>& children, std::wstring_view declaration, unsigned depth, bool grouped, bool cell, bool right, bool indexClose, bool stopBrace)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        const auto dimension = declaration == L"above" ? RuleDimensionSource() : std::wstring(0u, L' ');
        auto numerator       = FinishRow(children);
        auto denominator     = Row(depth, grouped, cell, right, indexClose, stopBrace, false);
        return FinishInfix(declaration, dimension, std::move(numerator), std::move(denominator));
    }
    MathNode DecorateTag(MathNode&& value, bool starred)
    {
        if (starred)
            return std::move(value);
        std::vector<MathNode> children(0u);
        auto left   = Make(MathKind::Text, L"(");
        left.italic = false;
        children.push_back(std::move(left));
        children.push_back(std::move(value));
        auto right   = Make(MathKind::Text, L")");
        right.italic = false;
        children.push_back(std::move(right));
        return Make(MathKind::Row, L"", std::move(children));
    }
    void ParseTag(unsigned depth, bool cell)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        const bool row = activeRowTags && cell && depth > activeRowTags->depth;
        if ((! row && (depth != 0u || equationTag)) ||
            (row && activeRowTags->tags &&
             std::ranges::any_of(activeRowTags->tags.value(), [&](const RowTagEntry& entry) { return entry.row == activeRowTags->row; })))
            Reject();
        const bool starred = at < source.size() && source[at] == L'*';
        if (starred)
            ++at;
        Space();
        if (at == source.size())
            Reject(MathIssue::MissingRequiredArgument);
        MathNode label;
        if (source[at] == L'{')
        {
            auto text    = GroupText(true, true);
            label        = Make(MathKind::Text, std::move(text));
            label.italic = false;
        }
        else
        {
            label        = Atom(depth + 1u);
            label.italic = false;
        }
        if (row && label.kind != MathKind::Text)
            Reject(MathIssue::UnsupportedCommand);
        if (row)
        {
            if (! activeRowTags->tags)
                activeRowTags->tags.emplace(0u);
            activeRowTags->tags.value().push_back({activeRowTags->row, DecorateTag(std::move(label), starred)});
            hasRowTags = true;
        }
        else
        {
            starredTag = starred;
            equationTag.emplace(std::move(label));
        }
    }
    MathNode ColorRow(std::vector<MathNode>& children, unsigned depth, bool grouped, bool cell, bool right, bool indexClose, bool stopBrace, bool infixAllowed)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (depth >= MathMaximumGroupDepth)
            Reject(MathIssue::Capacity);
        const auto color = Detail::ParseNativeColor(GroupText());
        if (! color)
            Reject(MathIssue::UnsupportedCommand);
        auto label = std::wstring(L"textcolor:");
        label += Detail::EncodeNativeColor(color.value());
        AppendColored(children, label, Row(depth + 1u, grouped, cell, right, indexClose, stopBrace, infixAllowed));
        return Make(MathKind::Row, L"", std::move(children));
    }
    MathNode FontSwitchRow(std::vector<MathNode>& children,
                           std::wstring_view command,
                           unsigned depth,
                           bool grouped,
                           bool cell,
                           bool right,
                           bool indexClose,
                           bool stopBrace,
                           bool infixAllowed)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (depth >= MathMaximumGroupDepth)
            Reject(MathIssue::Capacity);
        const auto style = command == L"bf"   ? L"mathbf"
                           : command == L"it" ? L"mathit"
                           : command == L"rm" ? L"mathrm"
                           : command == L"sf" ? L"mathsf"
                           : command == L"tt" ? L"mathtt"
                                              : L"mathcal";

        const auto first = at;
        auto body        = Row(depth + 1u, grouped, cell, right, indexClose, stopBrace, infixAllowed);
        if ((command == L"sf" || command == L"tt" || command == L"cal") && source.substr(first, at - first).find(L"\\text") != std::wstring_view::npos)
            Reject(MathIssue::UnsupportedCommand);
        if (command == L"sf" || command == L"tt" || command == L"cal")
            Alphabet(body, style);
        children.push_back(Make(MathKind::Style, std::wstring(style), {std::move(body)}));
        return Make(MathKind::Row, L"", std::move(children));
    }
    MathNode StyleRow(std::vector<MathNode>& children,
                      std::wstring_view declaration,
                      unsigned depth,
                      bool grouped,
                      bool cell,
                      bool right,
                      bool indexClose,
                      bool stopBrace,
                      bool infixAllowed)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (depth >= MathMaximumGroupDepth)
            Reject(MathIssue::Capacity);
        children.push_back(Make(MathKind::Style, std::wstring(declaration), {Row(depth + 1u, grouped, cell, right, indexClose, stopBrace, infixAllowed)}));
        return Make(MathKind::Row, L"", std::move(children));
    }
    void AppendRowAtom(std::vector<MathNode>& children, unsigned depth)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        auto atom = Atom(depth);
        Space();
        const bool hasScripts = ApplyScripts(atom, depth);
        if (! hasScripts && atom.kind == MathKind::Style && atom.text.starts_with(L"textcolor:") && atom.children.size() == 1u &&
            atom.children[0].kind == MathKind::Row)
        {
            const auto label = std::wstring(atom.text);
            AppendColored(children, label, std::move(atom.children[0]));
            return;
        }
        if (atom.kind == MathKind::Text && atom.atomClass == MathAtomClass::Ordinary && ! children.empty() && children.back().kind == MathKind::Text &&
            children.back().atomClass == MathAtomClass::Ordinary && children.back().italic == atom.italic)
            children.back().text.append(atom.text);
        else
            children.push_back(std::move(atom));
    }
    static bool IsInfixDeclaration(std::wstring_view value) noexcept
    {
        static constexpr std::wstring_view names[] = {L"over", L"atop", L"above", L"choose", L"brace", L"brack"};
        return std::ranges::find(names, value) != std::end(names);
    }
    static bool IsFontDeclaration(std::wstring_view value) noexcept
    {
        static constexpr std::wstring_view names[] = {L"bf", L"it", L"rm", L"sf", L"tt", L"cal"};
        return std::ranges::find(names, value) != std::end(names);
    }
    static bool IsSizeDeclaration(std::wstring_view value) noexcept
    {
        static constexpr std::wstring_view names[] = {L"displaystyle",
                                                      L"textstyle",
                                                      L"scriptstyle",
                                                      L"scriptscriptstyle",
                                                      L"tiny",
                                                      L"sixptsize",
                                                      L"scriptsize",
                                                      L"footnotesize",
                                                      L"small",
                                                      L"normalsize",
                                                      L"large",
                                                      L"Large",
                                                      L"LARGE",
                                                      L"huge",
                                                      L"Huge"};
        return std::ranges::find(names, value) != std::end(names);
    }
    MathNode ScopedRow(std::vector<MathNode>& children,
                       std::wstring_view declaration,
                       unsigned depth,
                       bool grouped,
                       bool cell,
                       bool right,
                       bool indexClose,
                       bool stopBrace,
                       bool infixAllowed)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        if (declaration == L"color")
            return ColorRow(children, depth, grouped, cell, right, indexClose, stopBrace, infixAllowed);
        if (IsFontDeclaration(declaration))
            return FontSwitchRow(children, declaration, depth, grouped, cell, right, indexClose, stopBrace, infixAllowed);
        return StyleRow(children, declaration, depth, grouped, cell, right, indexClose, stopBrace, infixAllowed);
    }
    MathNode Row(unsigned depth, bool grouped, bool cell, bool right = false, bool indexClose = false, bool stopBrace = false, bool infixAllowed = true)
    {
#if DXUI_ENABLE_DIAGNOSTICS
        ObserveStack();
#endif
        std::vector<MathNode> children(0u);
        while (at < source.size())
        {
            Space();
            if (at == source.size())
                break;
            if (indexClose && source[at] == L']')
            {
                ++at;
                return FinishRow(children);
            }
            if (right && source.substr(at, 6) == L"\\right" && (at + 6 == source.size() || ! Letter(source[at + 6])))
                break;
            if (cell && (source[at] == L'&' || RowSeparatorLength() != 0u || source.substr(at, 5) == L"\\end{"))
                break;
            if (stopBrace && source[at] == L'}')
                break;
            if (source[at] == L'}')
            {
                if (! grouped)
                    Reject();
                ++at;
                return FinishRow(children);
            }
            if (source[at] == L'\\')
            {
                const auto first = at + 1;
                auto end         = first;
                while (end < source.size() && Letter(source[end]))
                    ++end;
                const auto declaration = source.substr(first, end - first);
                if (declaration == L"nonumber" || declaration == L"notag")
                {
                    // This profile never synthesizes equation numbers; consume the
                    // declaration without adding an atom or changing manual tags.
                    at = end;
                    continue;
                }
                if (IsInfixDeclaration(declaration))
                {
                    if (! infixAllowed)
                        Reject(MathIssue::UnsupportedCommand);
                    at = end;
                    return InfixRow(children, declaration, depth, grouped, cell, right, indexClose, stopBrace);
                }
                if (declaration == L"tag")
                {
                    at = end;
                    ParseTag(depth, cell);
                    continue;
                }
                if (declaration == L"color" || IsFontDeclaration(declaration) || IsSizeDeclaration(declaration))
                {
                    at = end;
                    return ScopedRow(children, declaration, depth, grouped, cell, right, indexClose, stopBrace, infixAllowed);
                }
            }
            AppendRowAtom(children, depth);
        }
        if (grouped || indexClose)
            Reject(indexClose ? MathIssue::MissingRootIndexClose : MathIssue::MissingGroupClose);
        return FinishRow(children);
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
        if (hasRowTags)
            Reject(MathIssue::UnsupportedCommand);
        auto tag = DecorateTag(std::move(equationTag.value()), starredTag);
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
