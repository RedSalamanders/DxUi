#pragma once
#include "DxUi.MarkdownLabels.h"
#include "DxUi.MarkdownTableCells.h"
#include <DxUi/MarkdownSyntax.h>
#include <cwctype>

namespace DxUi::RichText::Detail
{
// Private block recognizer. It owns no source or host. The public parser owns
// staging and calls one physical-line transition at a time after scanning it.
class MarkdownBlocks
{
    using Kind = MarkdownBlockKind;
    std::wstring_view source_;
    std::vector<MarkdownNode>& nodes_;
    std::vector<MarkdownEmptyLine>& emptyLines_;
    unsigned containerIndent_{};
    unsigned activeFootnotes_{};
    std::vector<std::size_t> open_, tails_, previousSibling_;
    std::vector<bool> blank_;
    std::size_t leaf_ = MarkdownNoNode;
    std::size_t lineFirst_{}, lineEnd_{};
    std::size_t acceptedEnd_{};
    std::array<std::size_t, 3> lastOtherMarker_{};
    MarkdownBlockStatistics statistics_;

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
        if (sourceProfile_ != MarkdownProfile::RichMarkdown || (! interruptProbe && activeFootnotes_) || cursor.Indent() > 3u)
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
        if (cursor.Empty() || ! ValidFootnoteLabel(source_.substr(label, last - label)))
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
        const auto marker = cursor.Peek();
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
        if (text.size() > 2u && Starts(text, L"<!") && Letter(text[2]))
            return 4u;
        if (text.size() < 2u || text[0] != L'<')
            return 0;
        std::size_t first = text[1] == L'/' ? 2u : 1u, end = first;
        while (end < text.size() && (Letter(text[end]) || Digit(text[end]) || text[end] == L'-'))
            ++end;
        const auto name     = text.substr(first, end - first);
        const bool boundary = end == text.size() || Space(text[end]) || text[end] == L'>';
        const bool rawName  = EqualAscii(name, L"pre") || EqualAscii(name, L"script") || EqualAscii(name, L"style") ||
                              (sourceProfile_ != MarkdownProfile::Gfm029 && EqualAscii(name, L"textarea"));
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
                if (EqualAscii(name, known) && (known != L"search" || sourceProfile_ != MarkdownProfile::Gfm029))
                    return 6u;
        return ! paragraph && Tag(text) ? 7u : 0u;
    }
    bool HtmlEnd(unsigned type, std::wstring_view text) const
    {
        if (type == 1u)
        {
            for (std::size_t i = 0; i < text.size(); ++i)
                for (const auto tag : {L"</pre>", L"</script>", L"</style>", L"</textarea>"})
                    if ((sourceProfile_ != MarkdownProfile::Gfm029 || std::wstring_view(tag) != L"</textarea>") &&
                        i + std::wstring_view(tag).size() <= text.size() && EqualAscii(text.substr(i, std::wstring_view(tag).size()), tag))
                        return true;
            return false;
        }
        const std::wstring_view end = type == 2u ? L"-->" : type == 3u ? L"?>" : type == 4u ? L">" : L"]]>";
        return type <= 5u && text.find(end) != std::wstring_view::npos;
    }
    MarkdownProfile sourceProfile_;
    MarkdownExtensions extensions_;
    std::vector<MarkdownRange> Cells(MarkdownRange line) const
    {
        return MarkdownTableCells(source_, line);
    }
    void TableRow(std::size_t table, MarkdownContentLine line, const std::vector<MarkdownAlignment>& alignment, bool header)
    {
        const auto row            = Add(Kind::TableRow, table);
        nodes_[row].tableHeader   = header;
        nodes_[row].block.source  = {header ? nodes_[table].block.source.first : lineFirst_, line.source.last};
        nodes_[row].block.content = line.source;
        const auto cells          = Cells(line.source);
        for (std::size_t i = 0; i < alignment.size(); ++i)
        {
            const auto range          = i < cells.size() ? cells[i] : MarkdownRange{nodes_[row].block.content.last, nodes_[row].block.content.last};
            const auto cell           = Add(Kind::TableCell, row);
            nodes_[cell].alignment    = alignment[i];
            nodes_[cell].tableHeader  = header;
            nodes_[cell].block.source = nodes_[cell].block.content = range;
            nodes_[cell].lines.push_back({range, 0u});
        }
    }
    bool StartTable(Cursor cursor, std::size_t bodyEnd)
    {
        if (! HasMarkdownExtension(extensions_, MarkdownExtensions::Tables) || leaf_ == MarkdownNoNode || nodes_[leaf_].block.kind != Kind::Paragraph ||
            cursor.Indent() > 3u)
            return false;
        const auto rest = cursor.Rest();
        if (rest.find_first_not_of(L" |:-\t") != std::wstring_view::npos || rest.find(L'-') == std::wstring_view::npos)
            return false;
        References(leaf_);
        if (nodes_[leaf_].parent == MarkdownNoNode)
        {
            // An all-definition paragraph was replaced by reference nodes.
            // It supplies no table header and must never receive another line
            // or be extracted again through its retired parent sentinel.
            leaf_ = MarkdownNoNode;
            return false;
        }
        if (nodes_[leaf_].lines.size() != 1u)
            return false;
        const auto header = nodes_[leaf_].lines.front();
        if (rest.find(L'|') == std::wstring_view::npos &&
            source_.substr(header.source.first, header.source.last - header.source.first).find(L'|') == std::wstring_view::npos)
            return false;
        const auto delimiter = Cells({cursor.offset, bodyEnd});
        if (Cells(header.source).size() != delimiter.size())
            return false;
        std::vector<MarkdownAlignment> alignment(0u);
        for (const auto range : delimiter)
        {
            auto text       = source_.substr(range.first, range.last - range.first);
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
        const auto table                 = leaf_;
        nodes_[table].block.kind         = Kind::Table;
        nodes_[table].block.source.last  = lineEnd_;
        nodes_[table].block.content.last = bodyEnd;
        TableRow(table, header, alignment, true);
        return true;
    }
    void ContinueTable(Cursor cursor)
    {
        std::vector<MarkdownAlignment> alignment(0u);
        const auto header = nodes_[leaf_].firstChild;
        for (auto cell = nodes_[header].firstChild; cell != MarkdownNoNode; cell = nodes_[cell].nextSibling)
            alignment.push_back(nodes_[cell].alignment);
        TableRow(leaf_, {{cursor.offset, lineEnd_}, cursor.remaining}, alignment, false);
        nodes_[leaf_].block.source.last  = lineEnd_;
        nodes_[leaf_].block.content.last = lineEnd_;
    }
    std::size_t Add(Kind kind, std::size_t parent)
    {
        const auto index = nodes_.size();
        nodes_.emplace_back();
        tails_.push_back(MarkdownNoNode);
        previousSibling_.push_back(parent == MarkdownNoNode ? MarkdownNoNode : tails_[parent]);
        blank_.push_back(false);
        auto& node         = nodes_.back();
        node.block.kind    = kind;
        node.block.source  = {lineFirst_, lineEnd_};
        node.block.content = {lineFirst_, lineEnd_};
        node.parent        = parent;
        if (parent != MarkdownNoNode)
        {
            if (tails_[parent] != MarkdownNoNode)
                nodes_[tails_[parent]].nextSibling = index;
            else
                nodes_[parent].firstChild = index;
            tails_[parent] = index;
            if (kind != Kind::ListItem && blank_[parent] && nodes_[parent].block.kind == Kind::ListItem && previousSibling_[index] != MarkdownNoNode)
                nodes_[nodes_[parent].parent].tight = false;
            blank_[parent] = false;
        }
        return index;
    }
    void Content(std::size_t node, Cursor cursor, std::size_t bodyEnd, bool newline = true)
    {
        nodes_[node].lines.push_back({{cursor.offset, newline ? lineEnd_ : bodyEnd}, cursor.remaining});
        auto& block = nodes_[node].block;
        if (nodes_[node].lines.size() == 1u)
            block.content.first = cursor.offset;
        block.content.last = bodyEnd;
        if (block.kind == Kind::FencedCode || block.kind == Kind::IndentedCode || block.kind == Kind::HtmlBlock || block.kind == Kind::FrontMatter)
            block.content.last = lineEnd_;
        block.source.last = lineEnd_;
    }
    void Extend()
    {
        // Open containers share this accepted boundary. Updating every ancestor
        // on each lazy paragraph line would multiply depth by line count.
        acceptedEnd_ = lineEnd_;
    }
    void PopContainer()
    {
        const auto closing = open_.back();
        if (nodes_[closing].block.kind == Kind::BlockQuote || nodes_[closing].block.kind == Kind::ListItem)
            --containerIndent_;
        if (nodes_[closing].block.kind == Kind::FootnoteDefinition)
        {
            --activeFootnotes_;
            --containerIndent_;
            nodes_[closing].block.content.last = acceptedEnd_;
        }
        open_.pop_back();
        nodes_[closing].block.source.last = acceptedEnd_;
        ++statistics_.containerFinalizations;
        if (blank_[closing] && nodes_[closing].block.kind != Kind::BlockQuote)
            blank_[open_.back()] = true;
    }
    void CloseLeaf()
    {
        if (leaf_ != MarkdownNoNode && nodes_[leaf_].block.kind == Kind::Paragraph)
            References(leaf_);
        if (leaf_ != MarkdownNoNode && nodes_[leaf_].block.kind == Kind::IndentedCode)
        {
            auto& node = nodes_[leaf_];
            while (! node.lines.empty())
            {
                const auto& line = node.lines.back();
                auto text        = source_.substr(line.source.first, line.source.last - line.source.first);
                if (text.find_first_not_of(L" \t\r\n") != std::wstring_view::npos)
                    break;
                node.lines.pop_back();
            }
        }
        leaf_ = MarkdownNoNode;
    }
    bool FrontMatterLine(std::size_t first, std::size_t bodyEnd, std::size_t end)
    {
        if (sourceProfile_ != MarkdownProfile::RichMarkdown)
            return false;
        const auto body = source_.substr(first, bodyEnd - first);
        if (leaf_ != MarkdownNoNode && nodes_[leaf_].block.kind == Kind::FrontMatter)
        {
            auto& node        = nodes_[leaf_];
            const bool closes = node.frontMatterKind == MarkdownFrontMatterKind::Yaml ? body == L"---" || body == L"..." : body == L"+++";
            if (closes)
            {
                node.block.source.last  = end;
                node.block.content.last = first;
                node.block.closed       = true;
                leaf_                   = MarkdownNoNode;
            }
            else
                Content(leaf_, Cursor{source_, first, bodyEnd}, bodyEnd);
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
        leaf_                = Add(Kind::FrontMatter, 0u);
        auto& node           = nodes_[leaf_];
        node.frontMatterKind = marker == L"---" ? MarkdownFrontMatterKind::Yaml : MarkdownFrontMatterKind::Toml;
        node.block.content   = {end, end};
        node.block.info      = {bom, bodyEnd};
        node.block.closed    = false;
        Extend();
        return true;
    }
    bool ReferenceSpace(wchar_t c) const noexcept
    {
        return Space(c) || c == L'\r' || c == L'\n' || (sourceProfile_ == MarkdownProfile::Gfm029 && (c == L'\v' || c == L'\f'));
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
        if (sourceProfile_ == MarkdownProfile::RichMarkdown && i < text.size() && text[i] == L'^')
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
        const auto& original = nodes_[paragraph];
        if (original.lines.empty() || source_[original.lines.front().source.first] != L'[')
            return;
        std::wstring text(0u, L' ');
        std::vector<std::size_t> offsets(0u);
        for (const auto& line : original.lines)
            for (auto i = line.source.first; i < line.source.last; ++i)
            {
                offsets.push_back(i);
                if (source_[i] == L'\r')
                {
                    text.push_back(L'\n');
                    if (i + 1u < line.source.last && source_[i + 1u] == L'\n')
                        ++i;
                }
                else
                    text.push_back(source_[i]);
            }
        offsets.push_back(original.block.source.last);
        const auto parent  = original.parent;
        std::size_t cursor = 0, previous = MarkdownNoNode, lineCursor = 0u;
        const auto before = previousSibling_[paragraph];
        while (cursor < text.size())
        {
            const auto reference = ReferenceAt(text, cursor);
            if (! reference.end)
                break;
            const auto index = nodes_.size();
            nodes_.emplace_back();
            tails_.push_back(MarkdownNoNode);
            previousSibling_.push_back(previous == MarkdownNoNode ? before : previous);
            blank_.push_back(false);
            auto& node         = nodes_.back();
            node.parent        = parent;
            node.block.kind    = Kind::ReferenceDefinition;
            node.block.source  = {cursor ? offsets[cursor] : nodes_[paragraph].block.source.first, offsets[reference.end]};
            node.block.content = node.block.source;
            node.label         = {offsets[reference.labelFirst], offsets[reference.labelEnd]};
            node.destination   = {offsets[reference.destinationFirst], offsets[reference.destinationEnd]};
            node.title         = {offsets[reference.titleFirst], offsets[reference.titleEnd]};
            while (lineCursor < nodes_[paragraph].lines.size() && nodes_[paragraph].lines[lineCursor].source.last <= node.block.source.first)
                ++lineCursor;
            while (lineCursor < nodes_[paragraph].lines.size() && nodes_[paragraph].lines[lineCursor].source.first < node.block.source.last)
            {
                auto line         = nodes_[paragraph].lines[lineCursor++];
                line.source.first = std::max(line.source.first, node.block.source.first);
                line.source.last  = std::min(line.source.last, node.block.source.last);
                node.lines.push_back(line);
            }
            if (previous != MarkdownNoNode)
                nodes_[previous].nextSibling = index;
            else if (before != MarkdownNoNode)
                nodes_[before].nextSibling = index;
            else
                nodes_[parent].firstChild = index;
            previous = index;
            cursor   = reference.end;
        }
        if (previous == MarkdownNoNode)
            return;
        if (cursor == text.size())
        {
            nodes_[previous].nextSibling = nodes_[paragraph].nextSibling;
            if (nodes_[paragraph].nextSibling != MarkdownNoNode)
                previousSibling_[nodes_[paragraph].nextSibling] = previous;
            if (tails_[parent] == paragraph)
                tails_[parent] = previous;
            // Retired staging slots are not exposed; compaction follows at EOF.
            nodes_[paragraph].parent = MarkdownNoNode;
        }
        else
        {
            nodes_[previous].nextSibling = paragraph;
            previousSibling_[paragraph]  = previous;
            auto& node                   = nodes_[paragraph];
            node.block.source.first      = offsets[cursor];
            node.block.content.first     = offsets[cursor];
            const auto firstLine = std::find_if(node.lines.begin(), node.lines.end(), [&](const auto& line) { return line.source.last > offsets[cursor]; });
            node.lines.erase(node.lines.begin(), firstLine);
        }
    }

public:
    MarkdownBlocks(std::wstring_view source,
                   MarkdownProfile profile,
                   std::vector<MarkdownNode>& nodes,
                   std::vector<MarkdownEmptyLine>& emptyLines,
                   MarkdownExtensions extensions)
        : source_(source),
          nodes_(nodes),
          emptyLines_(emptyLines),
          open_(0u),
          tails_(0u),
          previousSibling_(0u),
          blank_(0u),
          sourceProfile_(profile),
          extensions_(extensions)
    {
        // Admission/copy already scans the bounded source. Reserve a physical-
        // line upper bound once so old/new publications cannot incur geometric
        // empty-line vector growth peaks during replacement. CRLF counts once.
        std::size_t physicalLines = 1u;
        for (std::size_t i = 0u; i < source.size(); ++i)
            if (source[i] == L'\r' || source[i] == L'\n')
            {
                ++physicalLines;
                if (source[i] == L'\r' && i + 1u < source.size() && source[i + 1u] == L'\n')
                    ++i;
            }
        emptyLines_.reserve(physicalLines);
        Add(Kind::Document, MarkdownNoNode);
        nodes_[0].block.source  = {0u, source.size()};
        nodes_[0].block.content = nodes_[0].block.source;
        open_.push_back(0u);
    }
    void Line(std::size_t first, std::size_t bodyEnd, std::size_t end)
    {
        lineFirst_ = first;
        lineEnd_   = end;
        if (FrontMatterLine(first, bodyEnd, end))
            return;
        lastOtherMarker_.fill(MarkdownNoNode);
        constexpr wchar_t markers[] = {L'-', L'*', L'_'};
        for (auto offset = first; offset < bodyEnd; ++offset)
        {
            ++statistics_.prefixInspectionUnits;
            if (! Space(source_[offset]))
                for (std::size_t marker = 0u; marker < 3u; ++marker)
                    if (source_[offset] != markers[marker])
                        lastOtherMarker_[marker] = offset;
        }
        Cursor cursor{source_, first, bodyEnd};
        cursor.inspections     = &statistics_.prefixInspectionUnits;
        cursor.lastOtherMarker = &lastOtherMarker_;
        std::size_t matched    = 1u;
        for (; matched < open_.size(); ++matched)
        {
            const auto& node = nodes_[open_[matched]];
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
        const bool paragraph = leaf_ != MarkdownNoNode && nodes_[leaf_].block.kind == Kind::Paragraph;
        if (matched < open_.size())
        {
            const auto marker = ListMarker(cursor);
            // Missing prefixes continue only an existing paragraph, and only
            // when this line cannot start an interrupting block in its own right.
            const bool listSibling = marker.valid && matched > 1u && nodes_[open_[matched - 1u]].block.kind == Kind::List;
            const bool interrupts  = listSibling || Thematic(cursor) || HtmlStart(cursor, true) || (cursor.Indent() <= 3u && trimmed.Peek() == L'>') ||
                                     Atx(cursor) || Fence(cursor) || FootnoteAt(cursor, true).valid ||
                                     (marker.valid && ! marker.empty && (! marker.ordered || marker.start == 1u));
            if (paragraph && ! empty && ! interrupts)
            {
                Content(leaf_, trimmed, bodyEnd);
                Extend();
                return;
            }
            CloseLeaf();
            while (open_.size() > matched)
                PopContainer();
        }
        if (leaf_ != MarkdownNoNode)
        {
            auto& node = nodes_[leaf_];
            if (node.block.kind == Kind::FencedCode)
            {
                auto candidate    = cursor;
                const auto indent = candidate.Indent();
                candidate.Spaces(indent);
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
                    leaf_                   = MarkdownNoNode;
                }
                else
                {
                    cursor.Spaces(std::min(cursor.Indent(), node.block.fenceIndent));
                    Content(leaf_, cursor, bodyEnd);
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
                    Content(leaf_, cursor, bodyEnd);
                    if (HtmlEnd(node.htmlType, cursor.Rest()))
                        leaf_ = MarkdownNoNode;
                    Extend();
                    return;
                }
            }
            if (leaf_ != MarkdownNoNode && nodes_[leaf_].block.kind == Kind::IndentedCode)
            {
                if (empty || cursor.Indent() >= 4u)
                {
                    cursor.Spaces(4u);
                    Content(leaf_, cursor, bodyEnd);
                    Extend();
                    return;
                }
                CloseLeaf();
            }
        }
        if (empty)
        {
            CloseLeaf();
            emptyLines_.push_back({{first, end}, bodyEnd, open_.back(), containerIndent_});
            blank_[open_.back()] = true;
            Extend();
            return;
        }
        if (leaf_ != MarkdownNoNode && nodes_[leaf_].block.kind == Kind::Table)
        {
            const auto marker = ListMarker(cursor);
            if (Thematic(cursor) || Atx(cursor) || Fence(cursor) || HtmlStart(cursor, false) || (cursor.Indent() <= 3u && trimmed.Peek() == L'>') ||
                marker.valid || FootnoteAt(cursor).valid)
                CloseLeaf();
            else
            {
                ContinueTable(trimmed);
                Extend();
                return;
            }
        }
        if (StartTable(cursor, bodyEnd))
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
                const auto quote = Add(Kind::BlockQuote, open_.back());
                open_.push_back(quote);
                ++containerIndent_;
                cursor = trimmed;
                cursor.Take();
                if (! cursor.Empty() && Space(cursor.Peek()))
                    cursor.Take();
                continue;
            }
            const auto marker      = ListMarker(cursor);
            const bool inParagraph = leaf_ != MarkdownNoNode && nodes_[leaf_].block.kind == Kind::Paragraph;
            if (marker.valid && ! Thematic(cursor) && (! inParagraph || (! marker.empty && (! marker.ordered || marker.start == 1u))))
            {
                CloseLeaf();
                auto parent = open_.back();
                if (nodes_[parent].block.kind != Kind::List || nodes_[parent].listMarker != marker.symbol)
                {
                    if (nodes_[parent].block.kind == Kind::List)
                    {
                        PopContainer();
                        parent = open_.back();
                    }
                    const auto list         = Add(Kind::List, parent);
                    nodes_[list].listMarker = marker.symbol;
                    nodes_[list].listStart  = marker.ordered ? marker.start : 0u;
                    open_.push_back(list);
                }
                const auto list = open_.back();
                if (blank_[list] && nodes_[list].firstChild != MarkdownNoNode)
                    nodes_[list].tight = false;
                const auto item            = Add(Kind::ListItem, list);
                nodes_[item].contentIndent = marker.indent;
                open_.push_back(item);
                ++containerIndent_;
                cursor = marker.content;
                continue;
            }
            if (nodes_[open_.back()].block.kind == Kind::List)
            {
                PopContainer();
                continue;
            }
            if (const auto note = FootnoteAt(cursor); note.valid)
            {
                CloseLeaf();
                const auto index            = Add(Kind::FootnoteDefinition, open_.back());
                nodes_[index].label         = note.label;
                nodes_[index].block.info    = note.marker;
                nodes_[index].block.content = {note.content.offset, end};
                nodes_[index].contentIndent = 4u;
                open_.push_back(index);
                ++activeFootnotes_;
                ++containerIndent_;
                cursor = note.content;
                continue;
            }
            break;
        }
        trimmed = cursor;
        trimmed.Trim();
        if (trimmed.Empty())
        {
            emptyLines_.push_back({{first, end}, bodyEnd, open_.back(), containerIndent_});
            Extend();
            return;
        }
        const bool continuing = leaf_ != MarkdownNoNode && nodes_[leaf_].block.kind == Kind::Paragraph;
        if (continuing)
        {
            if (const auto heading = Setext(cursor))
            {
                // Definitions precede setext interpretation; an all-definition
                // paragraph cannot turn the underline into a heading.
                References(leaf_);
                if (nodes_[leaf_].parent != MarkdownNoNode)
                {
                    nodes_[leaf_].block.kind        = Kind::SetextHeading;
                    nodes_[leaf_].block.heading     = heading;
                    nodes_[leaf_].block.source.last = end;
                    leaf_                           = MarkdownNoNode;
                    Extend();
                    return;
                }
                leaf_ = MarkdownNoNode;
            }
        }
        const auto indent = cursor.Indent();
        if (indent <= 3u)
        {
            const auto rest   = trimmed.Rest();
            const auto symbol = trimmed.Peek();
            std::size_t run   = 0;
            while (run < rest.size() && rest[run] == symbol)
                ++run;
            if (symbol == L'#' && run <= 6u && (run == rest.size() || Space(rest[run])))
            {
                CloseLeaf();
                const auto node            = Add(Kind::AtxHeading, open_.back());
                nodes_[node].block.heading = static_cast<unsigned>(run);
                for (std::size_t i = 0; i < run; ++i)
                    trimmed.Take();
                trimmed.Trim();
                auto last = bodyEnd;
                while (last > trimmed.offset && Space(source_[last - 1u]))
                    --last;
                auto hashes = last;
                while (hashes > trimmed.offset && source_[hashes - 1u] == L'#')
                    --hashes;
                if (hashes < last && (hashes == trimmed.offset || Space(source_[hashes - 1u])))
                {
                    last = hashes;
                    while (last > trimmed.offset && Space(source_[last - 1u]))
                        --last;
                }
                nodes_[node].block.content = {trimmed.offset, last};
                nodes_[node].lines.push_back({{trimmed.offset, last}, 0u});
                Extend();
                return;
            }
            if (Thematic(cursor))
            {
                CloseLeaf();
                Add(Kind::ThematicBreak, open_.back());
                Extend();
                return;
            }
            if ((symbol == L'`' || symbol == L'~') && run >= 3u && (symbol == L'~' || rest.substr(run).find(L'`') == std::wstring_view::npos))
            {
                CloseLeaf();
                leaf_             = Add(Kind::FencedCode, open_.back());
                auto& block       = nodes_[leaf_].block;
                block.fenceMarker = symbol;
                block.fenceLength = run;
                block.fenceIndent = indent;
                block.closed      = false;
                block.content     = {end, end};
                auto infoFirst = trimmed.offset + run, infoLast = bodyEnd;
                while (infoFirst < infoLast && Space(source_[infoFirst]))
                    ++infoFirst;
                while (infoLast > infoFirst && Space(source_[infoLast - 1u]))
                    --infoLast;
                block.info = {infoFirst, infoLast};
                Extend();
                return;
            }
            if (const auto html = HtmlStart(cursor, leaf_ != MarkdownNoNode))
            {
                CloseLeaf();
                leaf_                  = Add(Kind::HtmlBlock, open_.back());
                nodes_[leaf_].htmlType = html;
                Content(leaf_, cursor, bodyEnd);
                if (HtmlEnd(html, cursor.Rest()))
                    leaf_ = MarkdownNoNode;
                Extend();
                return;
            }
        }
        if (leaf_ != MarkdownNoNode)
        {
            Content(leaf_, trimmed, bodyEnd);
            Extend();
            return;
        }
        if (indent >= 4u)
        {
            leaf_ = Add(Kind::IndentedCode, open_.back());
            cursor.Spaces(4u);
            Content(leaf_, cursor, bodyEnd);
        }
        else
        {
            leaf_ = Add(Kind::Paragraph, open_.back());
            Content(leaf_, trimmed, bodyEnd);
        }
        Extend();
    }
    void Finish()
    {
        const bool trailingInLeaf = leaf_ != MarkdownNoNode && nodes_[leaf_].block.content.last == source_.size() &&
                                    (nodes_[leaf_].block.kind == Kind::FencedCode || nodes_[leaf_].block.kind == Kind::IndentedCode ||
                                     nodes_[leaf_].block.kind == Kind::HtmlBlock || nodes_[leaf_].block.kind == Kind::FrontMatter);
        CloseLeaf();
        for (const auto container : open_)
        {
            nodes_[container].block.source.last = acceptedEnd_;
            if (nodes_[container].block.kind == Kind::FootnoteDefinition)
                nodes_[container].block.content.last = acceptedEnd_;
            ++statistics_.containerFinalizations;
        }
        // Reference extraction retires a staging paragraph only after its
        // replacements exist. Compact once; published indices never change.
        std::vector<std::size_t> remap(nodes_.size(), MarkdownNoNode);
        std::size_t count = 0;
        for (std::size_t i = 0; i < nodes_.size(); ++i)
            if (! i || nodes_[i].parent != MarkdownNoNode)
                remap[i] = count++;
        for (std::size_t i = 0; i < nodes_.size(); ++i)
        {
            if (remap[i] == MarkdownNoNode)
                continue;
            auto& node = nodes_[i];
            if (node.parent != MarkdownNoNode)
                node.parent = remap[node.parent];
            if (node.firstChild != MarkdownNoNode)
                node.firstChild = remap[node.firstChild];
            if (node.nextSibling != MarkdownNoNode)
                node.nextSibling = remap[node.nextSibling];
            if (remap[i] != i)
                nodes_[remap[i]] = node;
        }
        nodes_.resize(count);
        for (auto& line : emptyLines_)
            line.owner = remap[line.owner];
        // The final source insertion line is real even when no block node owns
        // text there. Do not copy code-body blank lines: Content handled those.
        if (! trailingInLeaf && (source_.empty() || source_.back() == L'\r' || source_.back() == L'\n'))
            emptyLines_.push_back({{source_.size(), source_.size()}, source_.size(), 0u, 0u});
    }
    MarkdownBlockStatistics Statistics() const noexcept
    {
        return statistics_;
    }
};
} // namespace DxUi::RichText::Detail
