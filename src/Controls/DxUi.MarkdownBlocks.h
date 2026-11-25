#pragma once
#include "DxUi.MarkdownLabels.h"
#include "DxUi.MarkdownTableCells.h"
#include "DxUi.MarkdownWork.h"
#include "Support/WorkCounters.h"
#include <DxUi/Failure.h>
#include <DxUi/MarkdownSyntax.h>
#include <cwctype>
#include <optional>

namespace DxUi::RichText::Detail
{
// Private block recognizer. It owns no source or host. The public parser owns
// staging and calls one physical-line or generated-table-cell event at a time.
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4626 5027)
class MarkdownBlocks
{
    using Kind = MarkdownBlockKind;
    std::wstring_view _source;
    std::vector<MarkdownNode>& _nodes;
    std::vector<MarkdownEmptyLine>& _emptyLines;
    unsigned _containerIndent{};
    unsigned _activeFootnotes{};
    std::vector<std::size_t> _open, _tails, _previousSibling;
    std::vector<bool> _blank;
    std::size_t _leaf = MarkdownNoNode;
    std::size_t _lineFirst{}, _lineEnd{};
    std::size_t _acceptedEnd{};
    std::array<std::size_t, 3> _lastOtherMarker{};
    MarkdownBlockStatistics _statistics;
    struct TableRowPreparation
    {
        TableRowPreparation() : alignment(0u), cells(0u)
        {
        }
        std::size_t row{}, column{};
        std::vector<MarkdownAlignment> alignment;
        std::vector<MarkdownRange> cells;
    };
    std::optional<TableRowPreparation> _tableRow;
    std::optional<MarkdownSourceRegion> _sourceRegion;
    bool _sourceRegionStarted{};
    bool _emptyInlineRegion{};
    bool _capacityExceeded{};

    void StartSourceRegion(std::size_t bodyEnd)
    {
        CloseLeaf();
        _leaf = Add(Kind::SourceEditor, _open.back());
        if (_leaf == MarkdownNoNode)
            return;
        _sourceRegionStarted = true;
        Content(_leaf, Cursor{_source, _lineFirst, bodyEnd}, bodyEnd);
        Extend();
    }

    static bool Space(wchar_t c) noexcept
    {
        return c == L' ' || c == L'\t';
    }
    static bool Letter(wchar_t c) noexcept
    {
        return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
    }
    static bool Digit(wchar_t c) noexcept
    {
        return c >= L'0' && c <= L'9';
    }
    static wchar_t Lower(wchar_t c) noexcept
    {
        return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + 32) : c;
    }
    static bool EqualAscii(std::wstring_view a, std::wstring_view b) noexcept
    {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (Lower(a[i]) != b[i])
                return false;
        return true;
    }
    static bool Starts(std::wstring_view a, std::wstring_view b) noexcept
    {
        return a.starts_with(b);
    }
    // Columns, not UTF-16 offsets, govern prefix consumption. A tab may be
    // partly consumed by >, a list prefix or code indentation. Its remaining
    // virtual spaces are carried with the exact raw content span.
    struct Cursor
    {
        std::wstring_view text;
        std::size_t offset{}, end{};
        unsigned column{}, remaining{};
        std::size_t* inspections{};
        const std::array<std::size_t, 3>* lastOtherMarker{};
        bool Empty() const noexcept
        {
            return offset == end && ! remaining;
        }
        wchar_t Peek() const noexcept
        {
            return remaining ? L' ' : offset < end ? text[offset] : L'\0';
        }
        void Take() noexcept
        {
            if (inspections)
                ++*inspections;
            if (remaining)
            {
                --remaining;
                ++column;
            }
            else if (offset < end)
            {
                if (text[offset++] == L'\t')
                    remaining = 3u - column % 4u;
                ++column;
            }
        }
        unsigned Indent(unsigned maximum = 4u) const noexcept
        {
            auto copy        = *this;
            const auto first = column;
            while (copy.column - first < maximum && ! copy.Empty() && Space(copy.Peek()))
                copy.Take();
            return copy.column - first;
        }
        void Spaces(unsigned count) noexcept
        {
            while (count-- && ! Empty() && Space(Peek()))
                Take();
        }
        void Trim() noexcept
        {
            while (! Empty() && Space(Peek()))
                Take();
        }
        std::wstring_view Rest() const noexcept
        {
            return text.substr(offset, end - offset);
        }
    };
    struct FootnotePrefix
    {
        bool valid{};
        MarkdownRange label, marker;
        Cursor content;
    };
    FootnotePrefix FootnoteAt(Cursor cursor, bool interruptProbe = false) const
    {
        if (_sourceProfile != MarkdownProfile::RichMarkdown || (! interruptProbe && _activeFootnotes) || cursor.Indent() > 3u)
            return {};
        cursor.Trim();
        const auto first = cursor.offset;
        if (cursor.Peek() != L'[')
            return {};
        cursor.Take();
        if (cursor.Peek() != L'^')
            return {};
        cursor.Take();
        const auto label = cursor.offset;
        while (! cursor.Empty() && cursor.Peek() != L']')
        {
            if (cursor.Peek() == L'[')
                return {};
            const auto unit = cursor.Peek();
            cursor.Take();
            if (unit == L'\\' && (cursor.Peek() == L']' || cursor.Peek() == L'[' || cursor.Peek() == L'\\'))
            {
                cursor.Take();
            }
            else if (unit >= 0xd800 && unit <= 0xdbff && ! cursor.Empty())
                cursor.Take();
            // No valid 999-scalar UTF-16 label can exceed 1,998 units.
            if (cursor.offset - label > 1998u)
                return {};
        }
        const auto last = cursor.offset;
        if (cursor.Empty() || ! ValidFootnoteLabel(_source.substr(label, last - label)))
            return {};
        cursor.Take();
        if (cursor.Empty() || cursor.Peek() != L':')
            return {};
        cursor.Take();
        const auto markerEnd = cursor.offset;
        // Consume one optional column, preserving additional code indentation
        // and any virtual remainder of a tab for the ordinary block recognizer.
        if (! cursor.Empty() && Space(cursor.Peek()))
            cursor.Take();
        return {true, {label, last}, {first, markerEnd}, cursor};
    }
    struct Marker
    {
        bool valid{}, ordered{}, empty{};
        unsigned start{}, indent{};
        wchar_t symbol{};
        MarkdownRange source;
        Cursor content;
    };
    static Marker ListMarker(Cursor cursor)
    {
        Marker result;
        const auto first  = cursor.column;
        const auto indent = cursor.Indent();
        if (indent > 3u)
            return result;
        cursor.Spaces(indent);
        const auto marker      = cursor.Peek();
        const auto markerFirst = cursor.offset;
        if (marker == L'-' || marker == L'+' || marker == L'*')
        {
            result.symbol = marker;
            cursor.Take();
        }
        else if (Digit(marker))
        {
            result.ordered  = true;
            unsigned digits = 0;
            while (! cursor.Empty() && Digit(cursor.Peek()) && digits < 9u)
            {
                result.start = result.start * 10u + static_cast<unsigned>(cursor.Peek() - L'0');
                cursor.Take();
                ++digits;
            }
            if (cursor.Peek() != L'.' && cursor.Peek() != L')')
                return {};
            result.symbol = cursor.Peek();
            cursor.Take();
        }
        else
            return result;
        const auto markerLast = cursor.offset;
        if (! cursor.Empty() && ! Space(cursor.Peek()))
            return {};
        const auto afterMarker = cursor;
        const auto padding     = cursor.Indent(5u);
        cursor.Spaces(padding);
        auto remainder = cursor;
        if (padding > 4u)
            remainder.Trim();
        result.empty = remainder.Empty();
        if (result.empty || padding > 4u)
        {
            cursor = afterMarker;
            if (! cursor.Empty())
                cursor.Take();
        }
        result.indent  = cursor.column - first + (result.empty && padding == 0u ? 1u : 0u);
        result.source  = {markerFirst, markerLast};
        result.content = cursor;
        result.valid   = true;
        return result;
    }
    static bool Thematic(Cursor cursor)
    {
        if (cursor.Indent() > 3u)
            return false;
        cursor.Trim();
        const auto marker = cursor.Peek();
        unsigned count    = 0;
        if (marker != L'-' && marker != L'_' && marker != L'*')
            return false;
        const auto markerIndex = marker == L'-' ? 0u : marker == L'*' ? 1u : 2u;
        // A single line prepass makes suffix rejection constant-time. Repeated
        // '- ' container prefixes followed by text must not rescan that text at
        // every nesting level while testing thematic-break precedence.
        const auto other = (*cursor.lastOtherMarker)[markerIndex];
        if (other != MarkdownNoNode && other >= cursor.offset)
            return false;
        while (! cursor.Empty())
        {
            if (cursor.Peek() == marker)
                ++count;
            else if (! Space(cursor.Peek()))
                return false;
            cursor.Take();
        }
        return count >= 3u;
    }
    static bool Atx(Cursor cursor)
    {
        if (cursor.Indent() > 3u)
            return false;
        cursor.Trim();
        std::size_t count = 0u;
        while (! cursor.Empty() && cursor.Peek() == L'#')
        {
            cursor.Take();
            ++count;
        }
        return count >= 1u && count <= 6u && (cursor.Empty() || Space(cursor.Peek()));
    }
    static bool Fence(Cursor cursor)
    {
        if (cursor.Indent() > 3u)
            return false;
        cursor.Trim();
        const auto marker = cursor.Peek();
        if (marker != L'`' && marker != L'~')
            return false;
        std::size_t count = 0u;
        while (! cursor.Empty() && cursor.Peek() == marker)
        {
            cursor.Take();
            ++count;
        }
        return count >= 3u && (marker == L'~' || cursor.Rest().find(L'`') == std::wstring_view::npos);
    }
    bool DisplayMathFence(Cursor cursor) const
    {
        if (_sourceProfile != MarkdownProfile::RichMarkdown || cursor.Indent() > 3u)
            return false;
        cursor.Trim();
        const auto rest = cursor.Rest();
        return (rest.starts_with(L"$$") || rest.starts_with(L"\\[")) && std::ranges::all_of(rest.substr(2u), [](wchar_t c) { return c == L' ' || c == L'\t'; });
    }
    static unsigned Setext(Cursor cursor)
    {
        if (cursor.Indent() > 3u)
            return 0;
        cursor.Trim();
        const auto marker = cursor.Peek();
        if (marker != L'=' && marker != L'-')
            return 0;
        do
        {
            cursor.Take();
        } while (! cursor.Empty() && cursor.Peek() == marker);
        cursor.Trim();
        return cursor.Empty() ? marker == L'=' ? 1u : 2u : 0u;
    }
    static bool Tag(std::wstring_view text)
    {
        std::size_t i      = 1u;
        const bool closing = i < text.size() && text[i] == L'/';
        if (closing)
            ++i;
        if (i == text.size() || ! Letter(text[i]))
            return false;
        while (i < text.size() && (Letter(text[i]) || Digit(text[i]) || text[i] == L'-'))
            ++i;
        while (i < text.size())
        {
            const auto first = i;
            while (i < text.size() && Space(text[i]))
                ++i;
            if (i == text.size())
                return false;
            if (text[i] == L'>' || (! closing && text[i] == L'/' && i + 1u < text.size() && text[i + 1u] == L'>'))
            {
                i += text[i] == L'/' ? 2u : 1u;
                while (i < text.size() && Space(text[i]))
                    ++i;
                return i == text.size();
            }
            if (closing || first == i || ! (Letter(text[i]) || text[i] == L'_' || text[i] == L':'))
                return false;
            ++i;
            while (i < text.size() && (Letter(text[i]) || Digit(text[i]) || text[i] == L'_' || text[i] == L'.' || text[i] == L':' || text[i] == L'-'))
                ++i;
            const auto afterName = i;
            while (i < text.size() && Space(text[i]))
                ++i;
            if (i == text.size() || text[i] != L'=')
            {
                i = afterName;
                continue;
            }
            ++i;
            while (i < text.size() && Space(text[i]))
                ++i;
            if (i == text.size())
                return false;
            if (text[i] == L'\'' || text[i] == L'"')
            {
                const auto quote = text[i++];
                while (i < text.size() && text[i] != quote)
                    ++i;
                if (i == text.size())
                    return false;
                ++i;
            }
            else
            {
                const auto start = i;
                while (i < text.size() && ! Space(text[i]) && text[i] != L'>')
                {
                    if (text[i] == L'"' || text[i] == L'\'' || text[i] == L'=' || text[i] == L'<' || text[i] == L'`')
                        return false;
                    ++i;
                }
                if (start == i)
                    return false;
            }
        }
        return false;
    }
    unsigned HtmlStart(Cursor cursor, bool paragraph) const
    {
        if (cursor.Indent() > 3u)
            return 0;
        cursor.Trim();
        const auto text = cursor.Rest();
        if (Starts(text, L"<!--"))
            return 2u;
        if (Starts(text, L"<?"))
            return 3u;
        if (Starts(text, L"<![CDATA["))
            return 5u;
        if (text.size() > 2u && Starts(text, L"<!") && (_sourceProfile == MarkdownProfile::Gfm029 ? (text[2] >= L'A' && text[2] <= L'Z') : Letter(text[2])))
            return 4u;
        if (text.size() < 2u || text[0] != L'<')
            return 0;
        std::size_t first = text[1] == L'/' ? 2u : 1u, end = first;
        while (end < text.size() && (Letter(text[end]) || Digit(text[end]) || text[end] == L'-'))
            ++end;
        const auto name     = text.substr(first, end - first);
        const bool boundary = end == text.size() || Space(text[end]) || text[end] == L'>';
        const bool rawName  = EqualAscii(name, L"pre") || EqualAscii(name, L"script") || EqualAscii(name, L"style") ||
                              (_sourceProfile != MarkdownProfile::Gfm029 && EqualAscii(name, L"textarea"));
        if (first == 1u && rawName)
        {
            // Type 7 excludes these opening tag names, even when a complete
            // self-closing spelling failed type 1's boundary. Thus <pre/> is
            // paragraph source, whereas <pre /> starts a literal type-1 block.
            // Their complete closing tags may still start type 7.
            return boundary ? 1u : 0u;
        }
        constexpr std::wstring_view names[] = {
            L"address", L"article",  L"aside",    L"base",   L"basefont", L"blockquote", L"body",    L"caption",  L"center",     L"col",    L"colgroup",
            L"dd",      L"details",  L"dialog",   L"dir",    L"div",      L"dl",         L"dt",      L"fieldset", L"figcaption", L"figure", L"footer",
            L"form",    L"frame",    L"frameset", L"h1",     L"h2",       L"h3",         L"h4",      L"h5",       L"h6",         L"head",   L"header",
            L"hr",      L"html",     L"iframe",   L"legend", L"li",       L"link",       L"main",    L"menu",     L"menuitem",   L"nav",    L"noframes",
            L"ol",      L"optgroup", L"option",   L"p",      L"param",    L"search",     L"section", L"summary",  L"table",      L"tbody",  L"td",
            L"tfoot",   L"th",       L"thead",    L"title",  L"tr",       L"track",      L"ul"};
        if (boundary || (end + 1u < text.size() && text[end] == L'/' && text[end + 1u] == L'>'))
            for (const auto known : names)
                if (EqualAscii(name, known) && (known != L"search" || _sourceProfile != MarkdownProfile::Gfm029))
                    return 6u;
        return ! paragraph && Tag(text) ? 7u : 0u;
    }
    bool HtmlEnd(unsigned type, std::wstring_view text) const
    {
        if (type == 1u)
        {
            for (std::size_t i = 0; i < text.size(); ++i)
                for (const auto tag : {L"</pre>", L"</script>", L"</style>", L"</textarea>"})
                    if ((_sourceProfile != MarkdownProfile::Gfm029 || std::wstring_view(tag) != L"</textarea>") &&
                        i + std::wstring_view(tag).size() <= text.size() && EqualAscii(text.substr(i, std::wstring_view(tag).size()), tag))
                        return true;
            return false;
        }
        const std::wstring_view end = type == 2u ? L"-->" : type == 3u ? L"?>" : type == 4u ? L">" : L"]]>";
        return type <= 5u && text.find(end) != std::wstring_view::npos;
    }
    MarkdownProfile _sourceProfile;
    MarkdownExtensions _extensions;
    std::vector<MarkdownRange> Cells(MarkdownRange line) const
    {
        return MarkdownTableCells(_source, line);
    }
    void TableRow(std::size_t table, MarkdownContentLine line, const std::vector<MarkdownAlignment>& alignment, bool header)
    {
        // Padded cells amplify short sparse rows. Admit the complete row and
        // all its columns against the remaining document budget before any row
        // or cell allocation; subtraction avoids overflow in large products.
        if (alignment.size() >= MarkdownSyntax::MaximumNodeRecords - _nodes.size())
        {
            _capacityExceeded = true;
            return;
        }
        const auto row = Add(Kind::TableRow, table);
        if (row == MarkdownNoNode)
            return;
        _nodes[row].tableHeader   = header;
        _nodes[row].block.source  = {header ? _nodes[table].block.source.first : _lineFirst, line.source.last};
        _nodes[row].block.content = line.source;
        const auto cells          = Cells(line.source);
        _tableRow.emplace();
        _tableRow->row = row;
        _tableRow->alignment.assign(alignment.begin(), alignment.end());
        _tableRow->cells.assign(cells.begin(), cells.end());
    }
    bool StartTable(Cursor cursor, std::size_t bodyEnd)
    {
        if (! HasMarkdownExtension(_extensions, MarkdownExtensions::Tables) || _leaf == MarkdownNoNode || _nodes[_leaf].block.kind != Kind::Paragraph ||
            cursor.Indent() > 3u)
            return false;
        const auto rest = cursor.Rest();
        if (rest.find_first_not_of(L" |:-\t") != std::wstring_view::npos || rest.find(L'-') == std::wstring_view::npos)
            return false;
        if (_nodes[_leaf].lines.empty())
            return false;
        const auto candidateHeader = _nodes[_leaf].lines.back();
        if (rest.find(L'|') == std::wstring_view::npos &&
            _source.substr(candidateHeader.source.first, candidateHeader.source.last - candidateHeader.source.first).find(L'|') == std::wstring_view::npos)
            return false;
        const auto delimiter = Cells({cursor.offset, bodyEnd});
        if (Cells(candidateHeader.source).size() != delimiter.size())
            return false;
        std::vector<MarkdownAlignment> alignment(0u);
        for (const auto range : delimiter)
        {
            auto text       = _source.substr(range.first, range.last - range.first);
            const bool left = text.starts_with(L':'), right = text.ends_with(L':');
            if (left)
                text.remove_prefix(1u);
            if (right && ! text.empty())
                text.remove_suffix(1u);
            if (text.empty() || text.find_first_not_of(L'-') != std::wstring_view::npos)
                return false;
            alignment.push_back(left    ? right ? MarkdownAlignment::Center : MarkdownAlignment::Left
                                : right ? MarkdownAlignment::Right
                                        : MarkdownAlignment::None);
        }
        // A rejected delimiter must not re-flatten the growing paragraph.
        // Only a viable header can need reference extraction.
        References(_leaf);
        if (_capacityExceeded)
            return true;
        if (_nodes[_leaf].parent == MarkdownNoNode)
        {
            _leaf = MarkdownNoNode;
            return false;
        }
        if (_nodes[_leaf].lines.empty())
            return false;
        const auto header = _nodes[_leaf].lines.back();
        auto table        = _leaf;
        if (_nodes[_leaf].lines.size() > 1u)
        {
            const auto preceding = _nodes[_leaf].lines[_nodes[_leaf].lines.size() - 2u].source.last;
            auto contentEnd      = preceding;
            if (contentEnd > _nodes[_leaf].block.source.first && _source[contentEnd - 1u] == L'\n')
                --contentEnd;
            if (contentEnd > _nodes[_leaf].block.source.first && _source[contentEnd - 1u] == L'\r')
                --contentEnd;
            const auto parent = _nodes[_leaf].parent;
            _nodes[_leaf].lines.pop_back();
            _nodes[_leaf].block.source.last  = preceding;
            _nodes[_leaf].block.content.last = contentEnd;
            table                            = Add(Kind::Table, parent);
            if (table == MarkdownNoNode)
                return true;
            _nodes[table].block.source.first  = preceding;
            _nodes[table].block.content.first = header.source.first;
            _nodes[table].lines.push_back(header);
            _leaf = table;
        }
        _nodes[table].block.kind         = Kind::Table;
        _nodes[table].block.source.last  = _lineEnd;
        _nodes[table].block.content.last = bodyEnd;
        TableRow(table, header, alignment, true);
        return true;
    }
    void ContinueTable(Cursor cursor)
    {
        std::vector<MarkdownAlignment> alignment(0u);
        const auto header = _nodes[_leaf].firstChild;
        for (auto cell = _nodes[header].firstChild; cell != MarkdownNoNode; cell = _nodes[cell].nextSibling)
            alignment.push_back(_nodes[cell].alignment);
        TableRow(_leaf, {{cursor.offset, _lineEnd}, cursor.remaining}, alignment, false);
        if (_capacityExceeded)
            return;
        _nodes[_leaf].block.source.last  = _lineEnd;
        _nodes[_leaf].block.content.last = _lineEnd;
    }
    std::size_t Add(Kind kind, std::size_t parent)
    {
        if (_capacityExceeded || _nodes.size() >= MarkdownSyntax::MaximumNodeRecords)
        {
            _capacityExceeded = true;
            return MarkdownNoNode;
        }
        const auto index = _nodes.size();
        _nodes.emplace_back();
        _tails.push_back(MarkdownNoNode);
        _previousSibling.push_back(parent == MarkdownNoNode ? MarkdownNoNode : _tails[parent]);
        _blank.push_back(false);
        auto& node         = _nodes.back();
        node.block.kind    = kind;
        node.block.source  = {_lineFirst, _lineEnd};
        node.block.content = {_lineFirst, _lineEnd};
        node.parent        = parent;
        if (parent != MarkdownNoNode)
        {
            if (_tails[parent] != MarkdownNoNode)
                _nodes[_tails[parent]].nextSibling = index;
            else
                _nodes[parent].firstChild = index;
            _tails[parent] = index;
            if (kind != Kind::ListItem && _blank[parent] && _nodes[parent].block.kind == Kind::ListItem && _previousSibling[index] != MarkdownNoNode)
                _nodes[_nodes[parent].parent].tight = false;
            _blank[parent] = false;
        }
        return index;
    }
    void Content(std::size_t node, Cursor cursor, std::size_t bodyEnd, bool newline = true)
    {
        _nodes[node].lines.push_back({{cursor.offset, newline ? _lineEnd : bodyEnd}, cursor.remaining});
        auto& block = _nodes[node].block;
        if (_nodes[node].lines.size() == 1u)
            block.content.first = cursor.offset;
        block.content.last = bodyEnd;
        if (block.kind == Kind::FencedCode || block.kind == Kind::IndentedCode || block.kind == Kind::HtmlBlock || block.kind == Kind::FrontMatter ||
            block.kind == Kind::SourceEditor)
            block.content.last = _lineEnd;
        block.source.last = _lineEnd;
    }
    void Extend()
    {
        // Open containers share this accepted boundary. Updating every ancestor
        // on each lazy paragraph line would multiply depth by line count.
        _acceptedEnd = _lineEnd;
    }
    void PopContainer()
    {
        const auto closing = _open.back();
        if (_nodes[closing].block.kind == Kind::BlockQuote || _nodes[closing].block.kind == Kind::ListItem)
            --_containerIndent;
        if (_nodes[closing].block.kind == Kind::FootnoteDefinition)
        {
            --_activeFootnotes;
            --_containerIndent;
            _nodes[closing].block.content.last = _acceptedEnd;
        }
        _open.pop_back();
        _nodes[closing].block.source.last = _acceptedEnd;
        ++_statistics.containerFinalizations;
        if (_blank[closing] && _nodes[closing].block.kind != Kind::BlockQuote)
            _blank[_open.back()] = true;
    }
    void CloseLeaf()
    {
        if (_leaf != MarkdownNoNode && _nodes[_leaf].block.kind == Kind::Paragraph)
            References(_leaf);
        if (_leaf != MarkdownNoNode && _nodes[_leaf].block.kind == Kind::IndentedCode)
        {
            auto& node    = _nodes[_leaf];
            auto retained = node.lines.size();
            while (retained)
            {
                const auto& line = node.lines[retained - 1u];
                auto text        = _source.substr(line.source.first, line.source.last - line.source.first);
                if (text.find_first_not_of(L" \t\r\n") != std::wstring_view::npos)
                    break;
                --retained;
            }
            // CommonMark removes trailing code-body blank lines from rendered
            // code, but their raw insertion sites still belong to the editor.
            // Record them in source order before retiring the content records.
            for (auto index = retained; index < node.lines.size(); ++index)
            {
                const auto range = node.lines[index].source;
                auto first = range.first, bodyEnd = range.last;
                while (first && _source[first - 1u] != L'\r' && _source[first - 1u] != L'\n')
                    --first;
                while (bodyEnd > first && (_source[bodyEnd - 1u] == L'\r' || _source[bodyEnd - 1u] == L'\n'))
                    --bodyEnd;
                _emptyLines.push_back({{first, range.last}, bodyEnd, node.parent, _containerIndent});
            }
            node.lines.resize(retained);
        }
        _leaf = MarkdownNoNode;
    }
    bool FrontMatterLine(std::size_t first, std::size_t bodyEnd, std::size_t end)
    {
        if (_sourceProfile != MarkdownProfile::RichMarkdown)
            return false;
        const auto body = _source.substr(first, bodyEnd - first);
        if (_leaf != MarkdownNoNode && _nodes[_leaf].block.kind == Kind::FrontMatter)
        {
            auto& node        = _nodes[_leaf];
            const bool closes = node.frontMatterKind == MarkdownFrontMatterKind::Yaml ? body == L"---" || body == L"..." : body == L"+++";
            if (closes)
            {
                node.block.source.last  = end;
                node.block.content.last = first;
                node.block.closed       = true;
                _leaf                   = MarkdownNoNode;
            }
            else
                Content(_leaf, Cursor{_source, first, bodyEnd}, bodyEnd);
            Extend();
            return true;
        }
        // No indentation, leading blank line, container or later thematic break
        // can open metadata. An optional initial Unicode BOM remains in Source.
        if (first != 0u)
            return false;
        const auto bom    = body.starts_with(L'\ufeff') ? 1u : 0u;
        const auto marker = body.substr(bom);
        if (marker != L"---" && marker != L"+++")
            return false;
        _leaf = Add(Kind::FrontMatter, 0u);
        if (_leaf == MarkdownNoNode)
            return true;
        auto& node           = _nodes[_leaf];
        node.frontMatterKind = marker == L"---" ? MarkdownFrontMatterKind::Yaml : MarkdownFrontMatterKind::Toml;
        node.block.content   = {end, end};
        node.block.info      = {bom, bodyEnd};
        node.block.closed    = false;
        Extend();
        return true;
    }
    bool ReferenceSpace(wchar_t c) const noexcept
    {
        return Space(c) || c == L'\r' || c == L'\n' || (_sourceProfile == MarkdownProfile::Gfm029 && (c == L'\v' || c == L'\f'));
    }
    // Reference syntax is resolved only at the beginning of a paragraph. Keep
    // exact source spans; global Unicode folding/link resolution follows blocks.
    struct Reference
    {
        std::size_t end{}, labelFirst{}, labelEnd{}, destinationFirst{}, destinationEnd{}, titleFirst{}, titleEnd{};
    };
    Reference ReferenceAt(std::wstring_view text, std::size_t first) const
    {
        Reference result;
        auto i = first;
        while (i < text.size() && Space(text[i]))
            ++i;
        if (i == text.size() || text[i++] != L'[')
            return {};
        // RichMarkdown reserves this namespace even inside a definition, where
        // another note opener is literal rather than a nested/global link.
        if (_sourceProfile == MarkdownProfile::RichMarkdown && i < text.size() && text[i] == L'^')
            return {};
        result.labelFirst = i;
        unsigned count    = 0;
        bool nonblank     = false;
        while (i < text.size() && text[i] != L']')
        {
            if (text[i] == L'[' || (text[i] == L'\n' && i + 1u < text.size() && text[i + 1u] == L'\n'))
                return {};
            nonblank = nonblank || ! ReferenceSpace(text[i]);
            if (text[i] == L'\\' && i + 1u < text.size() && (text[i + 1u] == L'[' || text[i + 1u] == L']' || text[i + 1u] == L'\\'))
            {
                ++i;
                ++count;
            }
            if (text[i] >= 0xd800 && text[i] <= 0xdbff && i + 1u < text.size())
                ++i;
            ++i;
            if (++count > 999u)
                return {};
        }
        result.labelEnd = i;
        if (! nonblank || i + 1u >= text.size() || text[i++] != L']' || text[i++] != L':')
            return {};
        auto skip = [&](bool newline)
        {
            while (i < text.size() && ReferenceSpace(text[i]) && text[i] != L'\n')
                ++i;
            if (newline && i < text.size() && text[i] == L'\n')
            {
                ++i;
                while (i < text.size() && ReferenceSpace(text[i]) && text[i] != L'\n')
                    ++i;
            }
        };
        skip(true);
        if (i == text.size())
            return {};
        if (text[i] == L'<')
        {
            result.destinationFirst = ++i;
            while (i < text.size() && text[i] != L'>')
            {
                if (text[i] == L'\n' || text[i] == L'<')
                    return {};
                if (text[i] == L'\\' && i + 1u < text.size())
                    ++i;
                ++i;
            }
            if (i == text.size())
                return {};
            result.destinationEnd = i++;
        }
        else
        {
            result.destinationFirst = i;
            unsigned depth          = 0;
            while (i < text.size() && ! ReferenceSpace(text[i]))
            {
                if (text[i] < 0x20 || text[i] == 0x7f)
                    return {};
                if (text[i] == L'\\' && i + 1u < text.size() && ! ReferenceSpace(text[i + 1u]))
                {
                    i += 2u;
                    continue;
                }
                if (text[i] == L'(')
                    ++depth;
                else if (text[i] == L')')
                {
                    if (! depth)
                        return {};
                    --depth;
                }
                ++i;
            }
            if (depth || i == result.destinationFirst)
                return {};
            result.destinationEnd = i;
        }
        const auto afterDestination = i;
        while (i < text.size() && ReferenceSpace(text[i]) && text[i] != L'\n')
            ++i;
        const bool nextLine     = i < text.size() && text[i] == L'\n';
        const auto withoutTitle = i == text.size() ? i : nextLine ? i + 1u : 0u;
        if (nextLine)
        {
            ++i;
            while (i < text.size() && ReferenceSpace(text[i]) && text[i] != L'\n')
                ++i;
        }
        if (i > afterDestination && i < text.size() && (text[i] == L'\'' || text[i] == L'"' || text[i] == L'('))
        {
            const auto closing = text[i] == L'(' ? L')' : text[i];
            result.titleFirst  = ++i;
            bool valid         = true;
            while (i < text.size() && text[i] != closing)
            {
                if ((closing == L')' && text[i] == L'(') || (text[i] == L'\n' && i + 1u < text.size() && text[i + 1u] == L'\n'))
                {
                    valid = false;
                    break;
                }
                if (text[i] == L'\\' && i + 1u < text.size())
                    ++i;
                ++i;
            }
            result.titleEnd = i;
            if (i == text.size())
                valid = false;
            if (valid)
            {
                ++i;
                while (i < text.size() && ReferenceSpace(text[i]) && text[i] != L'\n')
                    ++i;
                valid = i == text.size() || text[i] == L'\n';
            }
            if (valid)
            {
                result.end = i < text.size() ? i + 1u : i;
                return result;
            }
            if (! nextLine)
                return {};
        }
        result.titleFirst = result.titleEnd = 0u;
        result.end                          = withoutTitle;
        return result;
    }
    void References(std::size_t paragraph)
    {
        // Flatten only this bounded paragraph's prefix-stripped spelling, never
        // the document. The offset oracle keeps CRLF and container prefixes exact.
        const auto& original = _nodes[paragraph];
        if (original.lines.empty() || original.lines.front().source.first >= original.lines.front().source.last ||
            original.lines.front().source.first >= _source.size() || _source[original.lines.front().source.first] != L'[')
            return;
        std::wstring text(0u, L' ');
        std::vector<std::size_t> offsets(0u);
        for (const auto& line : original.lines)
        {
            AccountMarkdownWork(&MarkdownWorkCounts::referenceFlattenUnits, line.source.last - line.source.first);
            ::DxUi::Detail::Work().rich.markdownReferenceUnitsRead += line.source.last - line.source.first;
            for (auto i = line.source.first; i < line.source.last; ++i)
            {
                offsets.push_back(i);
                if (_source[i] == L'\r')
                {
                    text.push_back(L'\n');
                    if (i + 1u < line.source.last && _source[i + 1u] == L'\n')
                        ++i;
                }
                else
                    text.push_back(_source[i]);
            }
        }
        offsets.push_back(original.block.source.last);
        const auto parent  = original.parent;
        std::size_t cursor = 0, previous = MarkdownNoNode, lineCursor = 0u;
        const auto before           = _previousSibling[paragraph];
        const auto physicalBoundary = [&](std::size_t offset)
        {
            const auto& lines = _nodes[paragraph].lines;
            const auto line   = std::lower_bound(lines.begin(), lines.end(), offset, [](const auto& entry, auto value) { return entry.source.first < value; });
            return line != lines.begin() && line != lines.end() && line->source.first == offset ? std::prev(line)->source.last : offset;
        };
        while (cursor < text.size())
        {
            const auto reference = ReferenceAt(text, cursor);
            if (! reference.end)
                break;
            // Reference extraction splices before the paragraph rather than using Add's tail linkage.
            // It must still share the complete block/inline graph admission ceiling.
            if (_nodes.size() >= MarkdownSyntax::MaximumNodeRecords)
            {
                _capacityExceeded = true;
                return;
            }
            const auto index = _nodes.size();
            _nodes.emplace_back();
            _tails.push_back(MarkdownNoNode);
            _previousSibling.push_back(previous == MarkdownNoNode ? before : previous);
            _blank.push_back(false);
            auto& node         = _nodes.back();
            node.parent        = parent;
            node.block.kind    = Kind::ReferenceDefinition;
            node.block.source  = {cursor ? physicalBoundary(offsets[cursor]) : _nodes[paragraph].block.source.first, physicalBoundary(offsets[reference.end])};
            node.block.content = node.block.source;
            node.label         = {offsets[reference.labelFirst], offsets[reference.labelEnd]};
            node.destination   = {offsets[reference.destinationFirst], offsets[reference.destinationEnd]};
            node.title         = {offsets[reference.titleFirst], offsets[reference.titleEnd]};
            while (lineCursor < _nodes[paragraph].lines.size() && _nodes[paragraph].lines[lineCursor].source.last <= node.block.source.first)
                ++lineCursor;
            while (lineCursor < _nodes[paragraph].lines.size() && _nodes[paragraph].lines[lineCursor].source.first < node.block.source.last)
            {
                auto line         = _nodes[paragraph].lines[lineCursor++];
                line.source.first = std::max(line.source.first, node.block.source.first);
                line.source.last  = std::min(line.source.last, node.block.source.last);
                node.lines.push_back(line);
            }
            if (previous != MarkdownNoNode)
                _nodes[previous].nextSibling = index;
            else if (before != MarkdownNoNode)
                _nodes[before].nextSibling = index;
            else
                _nodes[parent].firstChild = index;
            previous = index;
            cursor   = reference.end;
        }
        if (previous == MarkdownNoNode)
            return;
        if (cursor == text.size())
        {
            _nodes[previous].nextSibling = _nodes[paragraph].nextSibling;
            if (_nodes[paragraph].nextSibling != MarkdownNoNode)
                _previousSibling[_nodes[paragraph].nextSibling] = previous;
            if (_tails[parent] == paragraph)
                _tails[parent] = previous;
            // Retired staging slots are not exposed; compaction follows at EOF.
            _nodes[paragraph].parent = MarkdownNoNode;
        }
        else
        {
            _nodes[previous].nextSibling = paragraph;
            _previousSibling[paragraph]  = previous;
            auto& node                   = _nodes[paragraph];
            node.block.source.first      = physicalBoundary(offsets[cursor]);
            node.block.content.first     = offsets[cursor];
            const auto firstLine = std::find_if(node.lines.begin(), node.lines.end(), [&](const auto& line) { return line.source.last > offsets[cursor]; });
            node.lines.erase(node.lines.begin(), firstLine);
        }
    }

public:
    bool CapacityExceeded() const noexcept
    {
        return _capacityExceeded;
    }
    bool AdvanceTableCell()
    {
        if (! _tableRow)
            return false;
        auto& pending = *_tableRow;
        if (pending.column == pending.alignment.size())
        {
            _tableRow.reset();
            return true;
        }
        const auto column = pending.column++;
        const auto row    = pending.row;
        const auto end    = _nodes[row].block.content.last;
        const auto range  = column < pending.cells.size() ? pending.cells[column] : MarkdownRange{end, end};
        const auto cell   = Add(Kind::TableCell, row);
        if (cell == MarkdownNoNode)
            return true;
        _nodes[cell].alignment    = pending.alignment[column];
        _nodes[cell].tableHeader  = _nodes[row].tableHeader;
        _nodes[cell].block.source = _nodes[cell].block.content = range;
        _nodes[cell].lines.push_back({range, 0u});
        if (pending.column == pending.alignment.size())
            _tableRow.reset();
        return true;
    }
    MarkdownBlocks(std::wstring_view source,
                   MarkdownProfile profile,
                   std::vector<MarkdownNode>& nodes,
                   std::vector<MarkdownEmptyLine>& emptyLines,
                   MarkdownExtensions extensions,
                   std::optional<MarkdownSourceRegion> sourceRegion = std::nullopt,
                   bool emptyInlineRegion                           = false,
                   bool reserveLines                                = true)
        : _source(source),
          _nodes(nodes),
          _emptyLines(emptyLines),
          _open(0u),
          _tails(0u),
          _previousSibling(0u),
          _blank(0u),
          _sourceRegion(sourceRegion),
          _emptyInlineRegion(emptyInlineRegion),
          _sourceProfile(profile),
          _extensions(extensions)
    {
        // Reserve a physical-line upper bound once so old/new publications cannot
        // incur geometric empty-line vector growth peaks during replacement. CRLF
        // counts once. An incremental parser supplies the count instead (W7.2).
        if (reserveLines)
        {
            std::size_t physicalLines = 1u;
            for (std::size_t i = 0u; i < source.size(); ++i)
                if (source[i] == L'\r' || source[i] == L'\n')
                {
                    ++physicalLines;
                    if (source[i] == L'\r' && i + 1u < source.size() && source[i + 1u] == L'\n')
                        ++i;
                }
            _emptyLines.reserve(physicalLines);
        }
        Add(Kind::Document, MarkdownNoNode);
        _nodes[0].block.source  = {0u, source.size()};
        _nodes[0].block.content = _nodes[0].block.source;
        _open.push_back(0u);
    }
    void ReserveEmptyLines(std::size_t physicalLines)
    {
        _emptyLines.reserve(physicalLines);
    }
    // W7.2: a line starting here starts a top-level block exactly as it would with nothing open. Open containers do not
    // matter: a line that starts a top-level block matched none of them and closed them first. A document-level leaf or
    // table row does: the line could continue it, underline it or become its table header.
    bool StartsFresh() const noexcept
    {
        return ! _sourceRegion && (_open.size() > 1u || (_leaf == MarkdownNoNode && ! _tableRow));
    }
    bool CanImportClosedRoot() const noexcept
    {
        return ! _sourceRegion && _open.size() == 1u && _leaf == MarkdownNoNode && ! _tableRow;
    }
    bool CanImportNodeCount(std::size_t count) const noexcept
    {
        return count <= MarkdownSyntax::MaximumNodeRecords - _nodes.size();
    }
    std::size_t ImportClosedNode(const MarkdownNode& old, std::size_t parent, std::size_t oldFirst, std::size_t currentFirst)
    {
        const auto shift = [&](std::size_t at) noexcept { return currentFirst + (at - oldFirst); };
        const auto range = [&](MarkdownRange value) noexcept
        { return value.first == 0u && value.last == 0u ? value : MarkdownRange{shift(value.first), shift(value.last)}; };
        _lineFirst       = currentFirst;
        _lineEnd         = currentFirst + (old.block.source.last - oldFirst);
        const auto index = Add(old.block.kind, parent);
        if (index == MarkdownNoNode)
            return MarkdownNoNode;
        auto& node           = _nodes[index];
        node.block           = old.block;
        node.block.source    = {shift(old.block.source.first), shift(old.block.source.last)};
        node.block.content   = {shift(old.block.content.first), shift(old.block.content.last)};
        node.block.info      = range(old.block.info);
        node.label           = range(old.label);
        node.destination     = range(old.destination);
        node.title           = range(old.title);
        node.listStart       = old.listStart;
        node.contentIndent   = old.contentIndent;
        node.htmlType        = old.htmlType;
        node.listMarker      = old.listMarker;
        node.tight           = old.tight;
        node.alignment       = old.alignment;
        node.task            = old.task;
        node.tableHeader     = old.tableHeader;
        node.frontMatterKind = old.frontMatterKind;
        // Add constructed new parent/sibling links and an empty line vector.
        // Identity and inlines are rebuilt by the ordinary later passes.
        return index;
    }
    void ImportClosedLine(std::size_t node, const MarkdownContentLine& old, std::size_t oldFirst, std::size_t currentFirst)
    {
        _nodes[node].lines.push_back({{currentFirst + (old.source.first - oldFirst), currentFirst + (old.source.last - oldFirst)}, old.leadingSpaces});
    }
    void ImportClosedEmptyLine(const MarkdownEmptyLine& old, std::size_t owner, std::size_t oldFirst, std::size_t currentFirst)
    {
        _emptyLines.push_back({{currentFirst + (old.source.first - oldFirst), currentFirst + (old.source.last - oldFirst)},
                               currentFirst + (old.insertion - oldFirst),
                               owner,
                               old.indent});
    }
    // W7.2: a document-level blank line copied from the previous parse while nothing is open, exactly as Line records
    // it (owner document, the whole physical line, insertion before its line ending) without scanning it.
    void ImportDocumentBlankLine(const MarkdownEmptyLine& old, std::size_t oldFirst, std::size_t currentFirst)
    {
        const auto shift = [&](std::size_t at) noexcept { return currentFirst + (at - oldFirst); };
        _lineFirst       = shift(old.source.first);
        _lineEnd         = shift(old.source.last);
        _lastOtherMarker.fill(MarkdownNoNode);
        _emptyLines.push_back({{_lineFirst, _lineEnd}, shift(old.insertion), 0u, _containerIndent});
        _blank[0] = true;
        Extend();
    }
    void FinishClosedRoot(std::size_t end) noexcept
    {
        _acceptedEnd = end;
        ++_statistics.reusedClosedRoots;
    }
    void ImportClosedRootParagraph(const MarkdownNode& old, std::size_t currentFirst)
    {
        if (! CanImportClosedRoot() || old.parent != 0u || old.block.kind != Kind::Paragraph || old.firstChild != MarkdownNoNode ||
            old.block.source.first > old.block.source.last)
            DxUi::Failure::FailContract(E_INVALIDARG);
        const auto oldFirst   = old.block.source.first;
        const auto shift      = [&](std::size_t position) { return currentFirst + (position - oldFirst); };
        const auto shiftRange = [&](MarkdownRange range)
        { return range.first == 0u && range.last == 0u ? range : MarkdownRange{shift(range.first), shift(range.last)}; };
        _lineFirst       = currentFirst;
        _lineEnd         = shift(old.block.source.last);
        const auto index = Add(Kind::Paragraph, 0u);
        if (index == MarkdownNoNode)
            return;
        auto copy          = old;
        copy.block.source  = shiftRange(copy.block.source);
        copy.block.content = shiftRange(copy.block.content);
        copy.block.info    = shiftRange(copy.block.info);
        copy.label         = shiftRange(copy.label);
        copy.destination   = shiftRange(copy.destination);
        copy.title         = shiftRange(copy.title);
        for (auto& line : copy.lines)
            line.source = shiftRange(line.source);
        copy.parent     = 0u;
        copy.firstChild = copy.nextSibling = copy.firstInline = MarkdownNoNode;
        _nodes[index]                                         = copy;
        _acceptedEnd                                          = _lineEnd;
        ++_statistics.reusedClosedRoots;
    }
    void Line(std::size_t first, std::size_t bodyEnd, std::size_t end)
    {
        _lineFirst = first;
        _lineEnd   = end;
        // The editing envelope takes precedence over every grammar rule,
        // including its first line. Preserve the already open surrounding
        // containers, but never interpret typed fences, prefixes, definitions
        // or metadata inside the raw scope. A broken fence must stay repairable.
        if (_leaf != MarkdownNoNode && _nodes[_leaf].block.kind == Kind::SourceEditor)
        {
            if (first < _sourceRegion->source.last)
            {
                Content(_leaf, Cursor{_source, first, bodyEnd}, bodyEnd);
                Extend();
                return;
            }
            CloseLeaf();
        }
        const bool startsSourceRegion = _sourceRegion && ! _sourceRegionStarted && _sourceRegion->source.first >= first &&
                                        (_sourceRegion->source.first < end || (_sourceRegion->source.first == end && end == _source.size() && bodyEnd == end));
        if (! startsSourceRegion && FrontMatterLine(first, bodyEnd, end))
            return;
        _lastOtherMarker.fill(MarkdownNoNode);
        constexpr wchar_t markers[] = {L'-', L'*', L'_'};
        for (auto offset = first; offset < bodyEnd; ++offset)
        {
            ++_statistics.prefixInspectionUnits;
            if (! Space(_source[offset]))
                for (std::size_t marker = 0u; marker < 3u; ++marker)
                    if (_source[offset] != markers[marker])
                        _lastOtherMarker[marker] = offset;
        }
        Cursor cursor{_source, first, bodyEnd};
        cursor.inspections     = &_statistics.prefixInspectionUnits;
        cursor.lastOtherMarker = &_lastOtherMarker;
        std::size_t matched    = 1u;
        for (; matched < _open.size(); ++matched)
        {
            const auto& node = _nodes[_open[matched]];
            if (node.block.kind == Kind::List)
                continue;
            if (node.block.kind == Kind::BlockQuote)
            {
                if (cursor.Indent() > 3u)
                    break;
                auto candidate = cursor;
                candidate.Trim();
                if (candidate.Peek() != L'>')
                    break;
                candidate.Take();
                if (Space(candidate.Peek()) && ! candidate.Empty())
                    candidate.Take();
                cursor = candidate;
            }
            else
            {
                if (cursor.Indent(node.contentIndent) >= node.contentIndent)
                    cursor.Spaces(node.contentIndent);
                else
                {
                    auto candidate = cursor;
                    candidate.Trim();
                    if (! candidate.Empty() || (node.firstChild == MarkdownNoNode && node.block.kind != Kind::FootnoteDefinition))
                        break;
                    cursor = candidate;
                }
            }
        }
        auto trimmed = cursor;
        trimmed.Trim();
        const bool empty     = trimmed.Empty();
        const bool paragraph = _leaf != MarkdownNoNode && _nodes[_leaf].block.kind == Kind::Paragraph;
        if (matched < _open.size())
        {
            const auto marker = ListMarker(cursor);
            // Missing prefixes continue only an existing paragraph, and only
            // when this line cannot start an interrupting block in its own right.
            // The pinned GFM reference closes the unmatched quote before a
            // type-seven HTML block; CommonMark 0.31.2 keeps it lazy.
            const bool listSibling = marker.valid && matched > 1u && _nodes[_open[matched - 1u]].block.kind == Kind::List;
            const bool interrupts  = listSibling || Thematic(cursor) || HtmlStart(cursor, _sourceProfile != MarkdownProfile::Gfm029) ||
                                     (cursor.Indent() <= 3u && trimmed.Peek() == L'>') || Atx(cursor) || Fence(cursor) || DisplayMathFence(cursor) ||
                                     FootnoteAt(cursor, true).valid || marker.valid;
            if (paragraph && ! empty && ! interrupts && ! startsSourceRegion)
            {
                Content(_leaf, trimmed, bodyEnd);
                Extend();
                return;
            }
            CloseLeaf();
            while (_open.size() > matched)
                PopContainer();
        }
        if (startsSourceRegion)
        {
            CloseLeaf();
            // Only containers whose prefixes matched this physical line can
            // own an opaque editor. A leftover list shell has no current item.
            while (_nodes[_open.back()].block.kind == Kind::List)
                PopContainer();
            StartSourceRegion(bodyEnd);
            return;
        }
        if (_leaf != MarkdownNoNode)
        {
            auto& node = _nodes[_leaf];
            if (node.block.kind == Kind::FencedCode)
            {
                auto candidate    = cursor;
                const auto indent = candidate.Indent();
                candidate.Spaces(indent);
                if (node.block.fenceMarker == L'$' || node.block.fenceMarker == L'[')
                {
                    const auto delimiter = node.block.fenceMarker == L'$' ? std::wstring_view(L"$$") : std::wstring_view(L"\\]");
                    const auto rest      = candidate.Rest();
                    const bool closing   = indent <= 3u && rest.starts_with(delimiter) &&
                                           std::ranges::all_of(rest.substr(delimiter.size()), [](wchar_t c) { return c == L' ' || c == L'\t'; });
                    if (closing)
                    {
                        node.block.source.last  = end;
                        node.block.content.last = first;
                        node.block.closed       = true;
                        _leaf                   = MarkdownNoNode;
                    }
                    else
                    {
                        cursor.Spaces(std::min(cursor.Indent(), node.block.fenceIndent));
                        Content(_leaf, cursor, bodyEnd);
                    }
                    Extend();
                    return;
                }
                std::size_t count = 0u;
                while (! candidate.Empty() && candidate.Peek() == node.block.fenceMarker)
                {
                    ++count;
                    candidate.Take();
                }
                candidate.Trim();
                if (indent <= 3u && count >= node.block.fenceLength && candidate.Empty())
                {
                    node.block.source.last  = end;
                    node.block.content.last = first;
                    node.block.closed       = true;
                    _leaf                   = MarkdownNoNode;
                }
                else
                {
                    cursor.Spaces(std::min(cursor.Indent(), node.block.fenceIndent));
                    Content(_leaf, cursor, bodyEnd);
                }
                Extend();
                return;
            }
            if (node.block.kind == Kind::HtmlBlock)
            {
                if (node.htmlType >= 6u && empty)
                    CloseLeaf();
                else
                {
                    Content(_leaf, cursor, bodyEnd);
                    if (HtmlEnd(node.htmlType, cursor.Rest()))
                        _leaf = MarkdownNoNode;
                    Extend();
                    return;
                }
            }
            if (_leaf != MarkdownNoNode && _nodes[_leaf].block.kind == Kind::IndentedCode)
            {
                if (empty || cursor.Indent() >= 4u)
                {
                    cursor.Spaces(4u);
                    Content(_leaf, cursor, bodyEnd);
                    Extend();
                    return;
                }
                CloseLeaf();
            }
        }
        if (empty)
        {
            CloseLeaf();
            _emptyLines.push_back({{first, end}, bodyEnd, _open.back(), _containerIndent});
            _blank[_open.back()] = true;
            Extend();
            return;
        }
        if (_leaf != MarkdownNoNode && _nodes[_leaf].block.kind == Kind::Table)
        {
            const auto marker = ListMarker(cursor);
            if (Thematic(cursor) || Atx(cursor) || Fence(cursor) || DisplayMathFence(cursor) || HtmlStart(cursor, false) ||
                (cursor.Indent() <= 3u && trimmed.Peek() == L'>') || marker.valid || FootnoteAt(cursor).valid)
                CloseLeaf();
            else
            {
                ContinueTable(trimmed);
                Extend();
                return;
            }
        }
        if (StartTable(cursor, bodyEnd) || _capacityExceeded)
        {
            Extend();
            return;
        }
        while (true)
        {
            trimmed = cursor;
            trimmed.Trim();
            if (cursor.Indent() <= 3u && trimmed.Peek() == L'>')
            {
                CloseLeaf();
                const auto quote = Add(Kind::BlockQuote, _open.back());
                if (quote == MarkdownNoNode)
                    return;
                _open.push_back(quote);
                ++_containerIndent;
                cursor = trimmed;
                cursor.Take();
                if (! cursor.Empty() && Space(cursor.Peek()))
                    cursor.Take();
                continue;
            }
            const auto marker      = ListMarker(cursor);
            const bool inParagraph = _leaf != MarkdownNoNode && _nodes[_leaf].block.kind == Kind::Paragraph;
            if (marker.valid && ! Thematic(cursor) && (! inParagraph || (! marker.empty && (! marker.ordered || marker.start == 1u))))
            {
                CloseLeaf();
                auto parent = _open.back();
                if (_nodes[parent].block.kind != Kind::List || _nodes[parent].listMarker != marker.symbol)
                {
                    if (_nodes[parent].block.kind == Kind::List)
                    {
                        PopContainer();
                        parent = _open.back();
                    }
                    const auto list = Add(Kind::List, parent);
                    if (list == MarkdownNoNode)
                        return;
                    _nodes[list].listMarker = marker.symbol;
                    _nodes[list].listStart  = marker.ordered ? marker.start : 0u;
                    _open.push_back(list);
                }
                const auto list = _open.back();
                if (_blank[list] && _nodes[list].firstChild != MarkdownNoNode)
                    _nodes[list].tight = false;
                const auto item = Add(Kind::ListItem, list);
                if (item == MarkdownNoNode)
                    return;
                _nodes[item].block.info    = marker.source;
                _nodes[item].contentIndent = marker.indent;
                _open.push_back(item);
                ++_containerIndent;
                cursor = marker.content;
                continue;
            }
            if (_nodes[_open.back()].block.kind == Kind::List)
            {
                PopContainer();
                continue;
            }
            if (const auto note = FootnoteAt(cursor); note.valid)
            {
                CloseLeaf();
                const auto index = Add(Kind::FootnoteDefinition, _open.back());
                if (index == MarkdownNoNode)
                    return;
                _nodes[index].label         = note.label;
                _nodes[index].block.info    = note.marker;
                _nodes[index].block.content = {note.content.offset, end};
                _nodes[index].contentIndent = 4u;
                _open.push_back(index);
                ++_activeFootnotes;
                ++_containerIndent;
                cursor = note.content;
                continue;
            }
            break;
        }
        trimmed = cursor;
        trimmed.Trim();
        if (trimmed.Empty())
        {
            _emptyLines.push_back({{first, end}, bodyEnd, _open.back(), _containerIndent});
            Extend();
            return;
        }
        const bool continuing = _leaf != MarkdownNoNode && _nodes[_leaf].block.kind == Kind::Paragraph;
        if (continuing)
        {
            if (const auto heading = Setext(cursor))
            {
                // Definitions precede setext interpretation; an all-definition
                // paragraph cannot turn the underline into a heading.
                References(_leaf);
                if (_capacityExceeded)
                    return;
                if (_nodes[_leaf].parent != MarkdownNoNode)
                {
                    _nodes[_leaf].block.kind        = Kind::SetextHeading;
                    _nodes[_leaf].block.heading     = heading;
                    _nodes[_leaf].block.source.last = end;
                    _leaf                           = MarkdownNoNode;
                    Extend();
                    return;
                }
                _leaf = MarkdownNoNode;
            }
        }
        const auto indent = cursor.Indent();
        if (indent <= 3u)
        {
            const auto rest = trimmed.Rest();
            if (DisplayMathFence(cursor))
            {
                CloseLeaf();
                _leaf = Add(Kind::FencedCode, _open.back());
                if (_leaf == MarkdownNoNode)
                    return;
                auto& block       = _nodes[_leaf].block;
                block.fenceMarker = rest[0u] == L'$' ? L'$' : L'[';
                block.fenceLength = 2u;
                block.fenceIndent = indent;
                block.closed      = false;
                block.content     = {end, end};
                Extend();
                return;
            }
            const auto symbol = trimmed.Peek();
            std::size_t run   = 0;
            while (run < rest.size() && rest[run] == symbol)
                ++run;
            if (symbol == L'#' && run <= 6u && (run == rest.size() || Space(rest[run])))
            {
                CloseLeaf();
                const auto node = Add(Kind::AtxHeading, _open.back());
                if (node == MarkdownNoNode)
                    return;
                _nodes[node].block.heading = static_cast<unsigned>(run);
                for (std::size_t i = 0; i < run; ++i)
                    trimmed.Take();
                trimmed.Trim();
                auto last = bodyEnd;
                while (last > trimmed.offset && Space(_source[last - 1u]))
                    --last;
                auto hashes = last;
                while (hashes > trimmed.offset && _source[hashes - 1u] == L'#')
                    --hashes;
                if (hashes < last && (hashes == trimmed.offset || Space(_source[hashes - 1u])))
                {
                    last = hashes;
                    while (last > trimmed.offset && Space(_source[last - 1u]))
                        --last;
                }
                _nodes[node].block.content = {trimmed.offset, last};
                _nodes[node].lines.push_back({{trimmed.offset, last}, 0u});
                Extend();
                return;
            }
            if (Thematic(cursor))
            {
                CloseLeaf();
                Add(Kind::ThematicBreak, _open.back());
                Extend();
                return;
            }
            if ((symbol == L'`' || symbol == L'~') && run >= 3u && (symbol == L'~' || rest.substr(run).find(L'`') == std::wstring_view::npos))
            {
                CloseLeaf();
                _leaf = Add(Kind::FencedCode, _open.back());
                if (_leaf == MarkdownNoNode)
                    return;
                auto& block       = _nodes[_leaf].block;
                block.fenceMarker = symbol;
                block.fenceLength = run;
                block.fenceIndent = indent;
                block.closed      = false;
                block.content     = {end, end};
                auto infoFirst = trimmed.offset + run, infoLast = bodyEnd;
                while (infoFirst < infoLast && Space(_source[infoFirst]))
                    ++infoFirst;
                while (infoLast > infoFirst && Space(_source[infoLast - 1u]))
                    --infoLast;
                block.info = {infoFirst, infoLast};
                Extend();
                return;
            }
            if (const auto html = HtmlStart(cursor, _leaf != MarkdownNoNode))
            {
                CloseLeaf();
                _leaf = Add(Kind::HtmlBlock, _open.back());
                if (_leaf == MarkdownNoNode)
                    return;
                _nodes[_leaf].htmlType = html;
                Content(_leaf, cursor, bodyEnd);
                if (HtmlEnd(html, cursor.Rest()))
                    _leaf = MarkdownNoNode;
                Extend();
                return;
            }
        }
        if (_leaf != MarkdownNoNode)
        {
            Content(_leaf, trimmed, bodyEnd);
            Extend();
            return;
        }
        if (indent >= 4u)
        {
            _leaf = Add(Kind::IndentedCode, _open.back());
            if (_leaf == MarkdownNoNode)
                return;
            cursor.Spaces(4u);
            Content(_leaf, cursor, bodyEnd);
        }
        else
        {
            _leaf = Add(Kind::Paragraph, _open.back());
            if (_leaf == MarkdownNoNode)
                return;
            Content(_leaf, trimmed, bodyEnd);
        }
        Extend();
    }
    void Finish()
    {
        if (_emptyInlineRegion && _source.empty())
        {
            _lineFirst = _lineEnd = 0u;
            _leaf                 = Add(Kind::Paragraph, _open.back());
            if (_leaf == MarkdownNoNode)
                return;
            Content(_leaf, Cursor{_source, 0u, 0u}, 0u);
            Extend();
        }
        if (_sourceRegion && ! _sourceRegionStarted)
        {
            // Empty documents and the insertion line after a final EOL have
            // no Line event. Retain a real zero-span editable leaf at EOF.
            _lineFirst = _lineEnd = _source.size();
            StartSourceRegion(_source.size());
        }
        bool trailingInLeaf =
            _leaf != MarkdownNoNode && _nodes[_leaf].block.content.last == _source.size() &&
            (_nodes[_leaf].block.kind == Kind::FencedCode || _nodes[_leaf].block.kind == Kind::IndentedCode || _nodes[_leaf].block.kind == Kind::HtmlBlock ||
             _nodes[_leaf].block.kind == Kind::FrontMatter || _nodes[_leaf].block.kind == Kind::SourceEditor);
        CloseLeaf();
        if (! _emptyLines.empty() && _emptyLines.back().source.last == _source.size() && _emptyLines.back().source.first < _source.size())
            trailingInLeaf = false;
        for (const auto container : _open)
        {
            _nodes[container].block.source.last = _acceptedEnd;
            if (_nodes[container].block.kind == Kind::FootnoteDefinition)
                _nodes[container].block.content.last = _acceptedEnd;
            ++_statistics.containerFinalizations;
        }
        // Reference extraction retires a staging paragraph only after its
        // replacements exist. Compact once; published indices never change.
        std::vector<std::size_t> remap(_nodes.size(), MarkdownNoNode);
        std::size_t count = 0;
        for (std::size_t i = 0; i < _nodes.size(); ++i)
            if (! i || _nodes[i].parent != MarkdownNoNode)
                remap[i] = count++;
        for (std::size_t i = 0; i < _nodes.size(); ++i)
        {
            if (remap[i] == MarkdownNoNode)
                continue;
            auto& node = _nodes[i];
            if (node.parent != MarkdownNoNode)
                node.parent = remap[node.parent];
            if (node.firstChild != MarkdownNoNode)
                node.firstChild = remap[node.firstChild];
            if (node.nextSibling != MarkdownNoNode)
                node.nextSibling = remap[node.nextSibling];
            if (remap[i] != i)
                _nodes[remap[i]] = node;
        }
        _nodes.resize(count);
        for (auto& line : _emptyLines)
            line.owner = remap[line.owner];
        // The final source insertion line is real even when no block node owns
        // text there. Do not copy code-body blank lines: Content handled those.
        if (! trailingInLeaf && (_source.empty() || _source.back() == L'\r' || _source.back() == L'\n'))
            _emptyLines.push_back({{_source.size(), _source.size()}, _source.size(), 0u, 0u});
    }
    MarkdownBlockStatistics Statistics() const noexcept
    {
        return _statistics;
    }
};
#pragma warning(pop)
} // namespace DxUi::RichText::Detail
