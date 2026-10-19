#include "DxUi.MarkdownTableCells.h"
#include "DxUi.RichSemanticHtml.h"
#include <DxUi/RichTextCommands.h>
#include <limits>
#include <stdexcept>

namespace DxUi::RichText
{
MarkdownCommandPlan::MarkdownCommandPlan(const SourceSnapshot& source, std::uint64_t revision, std::shared_ptr<const MarkdownSyntax> syntax, Selection before)
    : source_(source),
      revision_(revision),
      syntax_(std::move(syntax)),
      before_(before),
      after_(before),
      edits_(0u)
{
}
namespace Detail
{
namespace
{
using Kind = MarkdownBlockKind;
bool Space(wchar_t c) noexcept
{
    return c == L' ' || c == L'\t';
}
bool Digit(wchar_t c) noexcept
{
    return c >= L'0' && c <= L'9';
}
unsigned Columns(std::wstring_view value, unsigned column = 0u) noexcept
{
    for (const auto c : value)
        column = c == L'\t' ? column + 4u - column % 4u : column + 1u;
    return column;
}
std::wstring ListMarker(bool ordered, unsigned number)
{
    // Keep every allocation fallible, including MSVC Debug iterator proxies.
    // Rvalue string concatenation enters a noexcept allocating move constructor.
    std::wstring value(0u, L' ');
    if (ordered)
    {
        value = std::to_wstring(number);
        value += L". ";
    }
    else
        value = L"- ";
    return std::wstring(value);
}
std::wstring HeadingMarker(unsigned level)
{
    std::wstring value(level, L'#');
    value += L' ';
    return std::wstring(value);
}
struct PhysicalLine
{
    std::size_t first{}, bodyEnd{}, end{};
};
struct ItemPrefix
{
    std::size_t first{}, markerEnd{}, content{};
    unsigned number{};
};
bool TextLeaf(Kind kind) noexcept
{
    return kind == Kind::Paragraph || kind == Kind::AtxHeading || kind == Kind::SetextHeading || kind == Kind::TableCell;
}
} // namespace
struct MarkdownCommandPlanner
{
    MarkdownCommandPlan plan;
    std::wstring_view text;
    const std::vector<MarkdownNode>& nodes;
    std::vector<PhysicalLine> lines;
    std::vector<MarkdownRange> opaqueInlines;
    bool explicitSelection{};

    MarkdownCommandPlanner(const SourceSnapshot& source, std::uint64_t revision, std::shared_ptr<const MarkdownSyntax> syntax, Selection selection)
        : plan(source, revision, std::move(syntax), selection),
          text(plan.syntax_ ? plan.syntax_->Source() : std::wstring_view{}),
          nodes(CheckedSyntax(source, revision, plan.syntax_, selection).Nodes()),
          lines(0u),
          opaqueInlines(0u)
    {
        for (std::size_t first = 0u;;)
        {
            auto end = first;
            while (end < text.size() && text[end] != L'\r' && text[end] != L'\n')
                ++end;
            const auto bodyEnd = end;
            if (end < text.size())
            {
                if (text[end++] == L'\r' && end < text.size() && text[end] == L'\n')
                    ++end;
            }
            lines.push_back({first, bodyEnd, end});
            if (end == text.size())
            {
                if (end > first && bodyEnd < end)
                    lines.push_back({end, end, end});
                break;
            }
            first = end;
        }
        for (const auto& node : plan.syntax_->Inlines())
            if (node.kind == MarkdownInlineKind::Code || node.kind == MarkdownInlineKind::Html)
                opaqueInlines.push_back(node.source);
        std::sort(opaqueInlines.begin(), opaqueInlines.end(), [](auto a, auto b) { return a.first < b.first; });
    }
    static const MarkdownSyntax& CheckedSyntax(const SourceSnapshot& source,
                                               std::uint64_t revision,
                                               const std::shared_ptr<const MarkdownSyntax>& syntax,
                                               Selection selection)
    {
        if (! syntax || syntax->Revision() != revision || syntax->Source().size() != source.Length() || source.Text() != syntax->Source())
            throw std::invalid_argument("Markdown command requires current source analysis");
        if (! source.IsBoundary(selection.anchor) || ! source.IsBoundary(selection.caret))
            throw std::out_of_range("Invalid Markdown command selection");
        return *syntax;
    }
    const PhysicalLine& Line(std::size_t at) const
    {
        const auto found = std::upper_bound(lines.begin(), lines.end(), at, [](auto position, const auto& line) { return position < line.first; });
        return *(found == lines.begin() ? found : std::prev(found));
    }
    std::wstring_view Slice(std::size_t first, std::size_t last) const
    {
        return text.substr(first, last - first);
    }
    std::wstring_view Eol(const PhysicalLine& line) const
    {
        if (line.end > line.bodyEnd)
            return Slice(line.bodyEnd, line.end);
        for (const auto& other : lines)
            if (other.end > other.bodyEnd)
                return Slice(other.bodyEnd, other.end);
        return L"\r\n";
    }
    void Add(std::size_t first, std::size_t last, std::wstring_view replacement)
    {
        if (first > last || ! plan.source_.IsBoundary(first) || ! plan.source_.IsBoundary(last))
            throw std::out_of_range("Invalid structural source edit");
        if (replacement.size() > Document::MaximumUnits || ! IsValidUtf16(replacement))
            throw std::invalid_argument("Invalid structural replacement");
        if (Slice(first, last) != replacement)
            plan.edits_.emplace_back(MarkdownRange{first, last}, replacement);
    }
    std::size_t NodeAt(Kind kind, std::size_t at) const
    {
        auto best = MarkdownNoNode;
        for (std::size_t i = 1u; i < nodes.size(); ++i)
        {
            const auto r = nodes[i].block.source;
            if (nodes[i].block.kind == kind && r.first <= at &&
                (at < r.last || (at == r.last && at == text.size() &&
                                 (text.empty() || (text.back() != L'\r' && text.back() != L'\n') || (kind == Kind::FencedCode && ! nodes[i].block.closed)))) &&
                (best == MarkdownNoNode || r.last - r.first <= nodes[best].block.source.last - nodes[best].block.source.first))
                best = i;
        }
        return best;
    }
    std::size_t LeafAt(std::size_t at) const
    {
        auto best = MarkdownNoNode;
        for (const auto kind : {Kind::Paragraph, Kind::AtxHeading, Kind::SetextHeading, Kind::TableCell})
        {
            const auto i = NodeAt(kind, at);
            if (i != MarkdownNoNode && (best == MarkdownNoNode || nodes[i].block.source.last - nodes[i].block.source.first <
                                                                      nodes[best].block.source.last - nodes[best].block.source.first))
                best = i;
        }
        return best;
    }
    std::size_t Ancestor(std::size_t node, Kind kind) const
    {
        for (auto i = node; i != MarkdownNoNode; i = nodes[i].parent)
            if (nodes[i].block.kind == kind)
                return i;
        return MarkdownNoNode;
    }
    std::size_t ParentItem(std::size_t item) const
    {
        return Ancestor(nodes[item].parent, Kind::ListItem);
    }
    std::size_t PreviousSibling(std::size_t node) const
    {
        if (node == MarkdownNoNode || nodes[node].parent == MarkdownNoNode)
            return MarkdownNoNode;
        auto previous = MarkdownNoNode;
        for (auto i = nodes[nodes[node].parent].firstChild; i != MarkdownNoNode; i = nodes[i].nextSibling)
        {
            if (i == node)
                return previous;
            previous = i;
        }
        return MarkdownNoNode;
    }
    // Only consumes container prefixes already proven by the syntax tree. A
    // partly consumed tab or missing lazy prefix has no editable raw boundary;
    // reject it instead of guessing a destructive substring.
    bool OuterOffset(std::size_t node, const PhysicalLine& line, std::size_t& at) const
    {
        std::vector<std::size_t> parents(0u);
        for (auto i = nodes[node].parent; i != MarkdownNoNode; i = nodes[i].parent)
            if (nodes[i].block.kind == Kind::BlockQuote || nodes[i].block.kind == Kind::ListItem)
                parents.push_back(i);
        at                   = line.first;
        unsigned column      = 0u;
        const auto takeSpace = [&](unsigned count)
        {
            const auto target = column + count;
            while (column < target && at < line.bodyEnd && Space(text[at]))
            {
                const auto next = text[at] == L'\t' ? column + 4u - column % 4u : column + 1u;
                if (next > target)
                    return false;
                column = next;
                ++at;
            }
            return column == target;
        };
        for (auto p = parents.rbegin(); p != parents.rend(); ++p)
        {
            const auto& owner = nodes[*p];
            if (owner.block.kind == Kind::ListItem && Line(owner.block.source.first).first != line.first)
            {
                if (! takeSpace(owner.contentIndent))
                    return false;
                continue;
            }
            while (at < line.bodyEnd && Space(text[at]))
            {
                column = text[at] == L'\t' ? column + 4u - column % 4u : column + 1u;
                ++at;
            }
            if (owner.block.kind == Kind::BlockQuote)
            {
                if (at == line.bodyEnd || text[at] != L'>')
                    return false;
                ++at;
                ++column;
                if (at < line.bodyEnd && Space(text[at]))
                {
                    column = text[at] == L'\t' ? column + 4u - column % 4u : column + 1u;
                    ++at;
                }
            }
            else
            {
                const auto marker = nodes[owner.parent].listMarker;
                while (at < line.bodyEnd && Digit(text[at]))
                {
                    ++at;
                    ++column;
                }
                if (at == line.bodyEnd || text[at] != marker)
                    return false;
                ++at;
                ++column;
                unsigned padding = 0u;
                auto end         = at;
                auto endColumn   = column;
                while (end < line.bodyEnd && Space(text[end]))
                {
                    endColumn = text[end] == L'\t' ? endColumn + 4u - endColumn % 4u : endColumn + 1u;
                    ++end;
                }
                padding = endColumn - column;
                if (padding > 4u || end == line.bodyEnd)
                    padding = padding ? 1u : 0u;
                if (! takeSpace(padding))
                    return false;
            }
        }
        return true;
    }
    bool Prefix(std::size_t item, ItemPrefix& prefix) const
    {
        const auto& line = Line(nodes[item].block.source.first);
        auto at          = line.first;
        if (! OuterOffset(item, line, at))
            return false;
        while (at < line.bodyEnd && Space(text[at]))
            ++at;
        prefix.first      = at;
        const auto marker = nodes[nodes[item].parent].listMarker;
        while (at < line.bodyEnd && Digit(text[at]))
        {
            prefix.number = prefix.number * 10u + static_cast<unsigned>(text[at] - L'0');
            ++at;
        }
        if (at == line.bodyEnd || text[at] != marker)
            return false;
        prefix.markerEnd = ++at;
        // The parser's first content line is stronger evidence than rescanning
        // padding, and retains task spelling before its inline label.
        auto first = nodes[item].firstChild;
        while (first != MarkdownNoNode && ! TextLeaf(nodes[first].block.kind) && nodes[first].firstChild != MarkdownNoNode)
            first = nodes[first].firstChild;
        if (first != MarkdownNoNode && ! nodes[first].lines.empty() && Line(nodes[first].lines.front().source.first).first == line.first)
            prefix.content = nodes[first].lines.front().source.first;
        else
        {
            auto end = at;
            while (end < line.bodyEnd && Space(text[end]))
                ++end;
            prefix.content = end - at <= 4u && end < line.bodyEnd ? end : at + (at < end ? 1u : 0u);
        }
        return prefix.content >= prefix.markerEnd && prefix.content <= line.bodyEnd;
    }
    std::size_t LabelStart(std::size_t item, const ItemPrefix& prefix) const
    {
        auto at = prefix.content;
        if (nodes[item].task != MarkdownTask::None && at + 3u <= text.size())
        {
            at += 3u;
            if (at < text.size() && Space(text[at]))
                ++at;
        }
        return at;
    }
    bool TaskLabelCanUnwrap(std::size_t item, const ItemPrefix& prefix) const
    {
        if (nodes[item].task == MarkdownTask::None)
            return true;
        const auto leaf = nodes[item].firstChild;
        if (leaf == MarkdownNoNode)
            return true;
        if (nodes[leaf].block.kind != Kind::Paragraph)
            return false;
        const auto firstInline = nodes[leaf].firstInline;
        if (firstInline == MarkdownNoNode)
            return true;
        const auto first = LabelStart(item, prefix), end = Line(first).bodyEnd;
        if (first == end)
            return true;
        // Removing [x] may expose a block marker that was literal in a task
        // paragraph. Do not silently reinterpret or reformat its label.
        if (Space(text[first]))
            return false;
        const auto& token = plan.syntax_->Inlines()[firstInline];
        if (token.kind == MarkdownInlineKind::Text)
        {
            if (std::wstring_view(L"-+*#><[_`|").find(text[first]) != std::wstring_view::npos)
                return false;
            auto at = first;
            while (at < end && Digit(text[at]))
                ++at;
            if (at > first && at - first <= 9u && at < end && (text[at] == L'.' || text[at] == L')') && (at + 1u == end || Space(text[at + 1u])))
                return false;
        }
        return token.kind != MarkdownInlineKind::Code || Line(token.source.first).first == Line(token.source.last).first;
    }
    std::vector<std::size_t> SelectedItems() const
    {
        std::vector<std::size_t> result(0u);
        const auto first = NodeAt(Kind::ListItem, plan.before_.Start());
        if (first == MarkdownNoNode)
            return result;
        auto end = plan.before_.End();
        if (end > plan.before_.Start() && Line(end).first == end)
            --end;
        for (auto i = first; i != MarkdownNoNode; i = nodes[i].nextSibling)
        {
            if (nodes[i].block.source.first > end)
                break;
            result.push_back(i);
            if (end < nodes[i].block.source.last || end == nodes[i].block.source.last)
                break;
        }
        if (result.empty() || end > nodes[result.back()].block.source.last)
            result.clear();
        return result;
    }
    bool ShiftItem(std::size_t item, int delta, bool removeMarker, std::size_t indentationOwner = MarkdownNoNode)
    {
        if (! TabShiftSafe(item, delta, ! removeMarker))
            return false;
        ItemPrefix prefix;
        if (! Prefix(item, prefix))
            return false;
        if (removeMarker && ! TaskLabelCanUnwrap(item, prefix))
            return false;
        const auto firstLine = Line(nodes[item].block.source.first).first;
        const auto lineIndex = static_cast<std::size_t>(&Line(firstLine) - lines.data());
        for (auto index = lineIndex; index < lines.size() && lines[index].first < nodes[item].block.source.last; ++index)
        {
            const auto& line = lines[index];
            std::size_t start{};
            if (! OuterOffset(indentationOwner == MarkdownNoNode ? item : indentationOwner, line, start))
                return false;
            if (line.first == firstLine && removeMarker)
            {
                Add(prefix.first, LabelStart(item, prefix), L"");
                continue;
            }
            auto end        = start;
            unsigned global = 0u;
            for (auto at = line.first; at < start; ++at)
                global = text[at] == L'\t' ? global + 4u - global % 4u : global + 1u;
            auto column = global;
            while (end < line.bodyEnd && Space(text[end]))
            {
                column = text[end] == L'\t' ? column + 4u - column % 4u : column + 1u;
                ++end;
            }
            const auto columns = column - global;
            if (delta < 0 && columns < static_cast<unsigned>(-delta))
            {
                if (end == line.bodyEnd)
                    continue;
                return false;
            }
            const auto count = delta < 0 ? columns - static_cast<unsigned>(-delta) : columns + static_cast<unsigned>(delta);
            Add(start, end, std::wstring(count, L' '));
        }
        return true;
    }
    bool ShiftFollowing(std::size_t item, const PhysicalLine& opening, int delta)
    {
        if (! delta)
            return true;
        if (! TabShiftSafe(item, delta, false))
            return false;
        for (auto index = static_cast<std::size_t>(&Line(opening.first) - lines.data()) + 1u;
             index < lines.size() && lines[index].first < nodes[item].block.source.last;
             ++index)
        {
            const auto& line = lines[index];
            std::size_t start{};
            if (! OuterOffset(item, line, start))
                return false;
            auto end        = start;
            unsigned column = 0u;
            for (auto at = line.first; at < start; ++at)
                column = text[at] == L'\t' ? column + 4u - column % 4u : column + 1u;
            const auto initial = column;
            while (end < line.bodyEnd && Space(text[end]))
            {
                column = text[end] == L'\t' ? column + 4u - column % 4u : column + 1u;
                ++end;
            }
            if (column - initial < nodes[item].contentIndent)
            {
                if (end == line.bodyEnd)
                    continue;
                return false;
            }
            Add(start, end, std::wstring(static_cast<std::size_t>(static_cast<int>(column - initial) + delta), L' '));
        }
        return true;
    }
    bool TabShiftSafe(std::size_t item, int delta, bool includeSelf) const
    {
        if (delta % 4 == 0)
            return true;
        std::vector<std::size_t> pending(1u, item);
        while (! pending.empty())
        {
            const auto index = pending.back();
            pending.pop_back();
            if (nodes[index].block.kind == Kind::ListItem && (includeSelf || index != item))
            {
                ItemPrefix prefix;
                if (! Prefix(index, prefix))
                    return false;
                if (Slice(prefix.first, prefix.content).find(L'\t') != std::wstring_view::npos)
                    return false;
            }
            for (auto child = nodes[index].firstChild; child != MarkdownNoNode; child = nodes[child].nextSibling)
                pending.push_back(child);
        }
        return true;
    }
    void Indent(bool outdent)
    {
        const auto items = SelectedItems();
        if (items.empty())
            return;
        const auto parent = ParentItem(items.front());
        int delta{};
        bool remove = false;
        if (outdent)
        {
            remove = parent == MarkdownNoNode;
            delta  = -static_cast<int>(nodes[remove ? items.front() : parent].contentIndent);
        }
        else
        {
            const auto previous = PreviousSibling(items.front());
            if (previous == MarkdownNoNode)
                return;
            delta = static_cast<int>(nodes[previous].contentIndent);
        }
        for (const auto item : items)
            if (! ShiftItem(item, delta, remove, outdent && ! remove ? parent : MarkdownNoNode))
            {
                plan.edits_.clear();
                return;
            }
        plan.applicable_ = true;
    }
    void Continue()
    {
        const auto first = plan.before_.Start(), last = plan.before_.End();
        const auto& line = Line(first);
        if (Line(last).first != line.first)
            return;
        for (const auto kind : {Kind::FencedCode, Kind::IndentedCode, Kind::HtmlBlock})
        {
            const auto opaque = NodeAt(kind, first);
            if (opaque == MarkdownNoNode)
                continue;
            std::size_t at{};
            if (! OuterOffset(opaque, line, at))
                return;
            for (const auto& content : nodes[opaque].lines)
                if (Line(content.source.first).first == line.first)
                {
                    at = content.source.first;
                    break;
                }
            std::wstring inserted(Eol(line));
            inserted += Slice(line.first, at);
            Add(first, last, inserted);
            plan.after_       = {first + inserted.size(), first + inserted.size()};
            explicitSelection = true;
            plan.applicable_  = true;
            return;
        }
        const auto item = NodeAt(Kind::ListItem, first);
        std::wstring prefix(0u, L' ');
        if (item != MarkdownNoNode)
        {
            ItemPrefix marker;
            if (! Prefix(item, marker))
                return;
            std::size_t outer{};
            if (! OuterOffset(item, line, outer))
                return;
            const auto opening = Line(nodes[item].block.source.first);
            const auto label   = LabelStart(item, marker);
            if (line.first == opening.first)
            {
                if (first < label)
                    return;
                auto end = line.bodyEnd;
                while (end > label && Space(text[end - 1u]))
                    --end;
                const auto child = nodes[item].firstChild;
                if (end == label && (child == MarkdownNoNode || (nodes[child].block.kind == Kind::Paragraph && nodes[child].nextSibling == MarkdownNoNode)))
                {
                    Indent(true);
                    return;
                }
            }
            std::size_t openingOuter{};
            if (! OuterOffset(item, opening, openingOuter))
                return;
            prefix.assign(Slice(line.first, outer));
            prefix += Slice(openingOuter, marker.first);
            const auto symbol      = nodes[nodes[item].parent].listMarker;
            const auto markerStart = prefix.size();
            if (symbol == L'.' || symbol == L')')
            {
                if (marker.number == 999999999u)
                    return;
                prefix += std::to_wstring(marker.number + 1u);
                prefix += symbol;
            }
            else
                prefix += symbol;
            prefix += Slice(marker.markerEnd, marker.content);
            if (prefix.empty() || ! Space(prefix.back()))
                prefix += L' ';
            // Crossing 9 -> 10 (or another digit boundary) moves the new item's
            // remaining continuation/subtree indentation with its owned marker.
            const auto oldColumn = Columns(Slice(opening.first, marker.first));
            const auto newColumn = Columns(std::wstring_view(prefix).substr(0u, markerStart));
            const auto oldWidth  = Columns(Slice(marker.first, marker.content), oldColumn) - oldColumn;
            const auto newWidth  = Columns(std::wstring_view(prefix).substr(markerStart), newColumn) - newColumn;
            if (! ShiftFollowing(item, line, static_cast<int>(newWidth) - static_cast<int>(oldWidth)))
                return;
            if (nodes[item].task != MarkdownTask::None)
                prefix += L"[ ] ";
        }
        else
        {
            const auto leaf = LeafAt(first);
            if (leaf != MarkdownNoNode && nodes[leaf].block.kind != Kind::Paragraph && nodes[leaf].block.kind != Kind::AtxHeading &&
                nodes[leaf].block.kind != Kind::SetextHeading)
                return;
            if (leaf != MarkdownNoNode)
            {
                std::size_t at{};
                if (! OuterOffset(leaf, line, at))
                    return;
                prefix.assign(Slice(line.first, at));
            }
            else
            {
                // Empty quote-only lines retain their exact prefix. A second
                // Enter exits one quote level without manufacturing a raw ACP.
                const auto quote = NodeAt(Kind::BlockQuote, first);
                if (quote != MarkdownNoNode)
                {
                    auto at = line.first;
                    while (at < line.bodyEnd && (Space(text[at]) || text[at] == L'>'))
                        ++at;
                    if (first < at)
                        return;
                    const auto marker = text.rfind(L'>', at ? at - 1u : 0u);
                    if (marker >= line.first && marker < at)
                    {
                        Add(marker, at, L"");
                        plan.applicable_ = true;
                        return;
                    }
                }
            }
        }
        std::wstring inserted(Eol(line));
        inserted += prefix;
        Add(first, last, inserted);
        plan.after_       = {first + inserted.size(), first + inserted.size()};
        explicitSelection = true;
        plan.applicable_  = true;
    }
    void Heading(unsigned level)
    {
        if (level > 6u)
            throw std::invalid_argument("Heading level must be zero through six");
        if (HasOpaqueSelection())
            return;
        auto end = plan.before_.End();
        if (end > plan.before_.Start() && Line(end).first == end)
            --end;
        const auto active = plan.before_.Start() == plan.before_.End() ? LeafAt(plan.before_.caret) : MarkdownNoNode;
        bool found        = false;
        for (std::size_t i = 1u; i < nodes.size(); ++i)
        {
            const auto& node = nodes[i];
            const auto kind  = node.block.kind;
            if (kind != Kind::Paragraph && kind != Kind::AtxHeading && kind != Kind::SetextHeading)
                continue;
            if (node.block.source.last < plan.before_.Start() || node.block.source.first > end)
                continue;
            if (plan.before_.Start() == plan.before_.End() && i != active)
                continue;
            if (node.lines.empty())
                continue;
            const auto item = Ancestor(i, Kind::ListItem);
            if (level && item != MarkdownNoNode && nodes[item].task != MarkdownTask::None)
            {
                plan.edits_.clear();
                return;
            }
            found = true;
            if (kind == Kind::SetextHeading && level > 0u && level <= 2u)
            {
                const auto& underline = Line(node.lines.back().source.last);
                std::size_t start{};
                if (! OuterOffset(i, underline, start))
                {
                    plan.edits_.clear();
                    return;
                }
                while (start < underline.bodyEnd && Space(text[start]))
                    ++start;
                auto last = start;
                while (last < underline.bodyEnd && (text[last] == L'=' || text[last] == L'-'))
                    ++last;
                Add(start, last, std::wstring(std::max<std::size_t>(3u, last - start), level == 1u ? L'=' : L'-'));
                continue;
            }
            if (kind == Kind::SetextHeading)
            {
                const auto& underline = Line(node.lines.back().source.last);
                Add(underline.first, underline.end, L"");
            }
            for (const auto& content : node.lines)
            {
                const auto& line = Line(content.source.first);
                std::size_t start{};
                if (! OuterOffset(i, line, start))
                {
                    plan.edits_.clear();
                    return;
                }
                while (start < line.bodyEnd && Space(text[start]))
                    ++start;
                auto markerEnd = start;
                if (kind == Kind::AtxHeading)
                {
                    while (markerEnd < line.bodyEnd && text[markerEnd] == L'#')
                        ++markerEnd;
                    if (level == 0u)
                    {
                        while (markerEnd < line.bodyEnd && Space(text[markerEnd]))
                            ++markerEnd;
                        auto close = line.bodyEnd;
                        while (close > node.block.content.last && Space(text[close - 1u]))
                            --close;
                        auto closeFirst = close;
                        while (closeFirst > node.block.content.last && text[closeFirst - 1u] == L'#')
                            --closeFirst;
                        if (closeFirst < close)
                            Add(closeFirst, close, L"");
                    }
                    Add(start, markerEnd, std::wstring(level, L'#'));
                }
                else if (level)
                    Add(start, start, HeadingMarker(level));
            }
        }
        if (! found && EmptyStructuralSite())
        {
            if (level)
                Add(plan.before_.caret, plan.before_.caret, HeadingMarker(level));
            found = true;
        }
        plan.applicable_ = found;
    }
    void SetList(const MarkdownCommandRequest& request);
    void Join(bool backwards);
    void Style(MarkdownTextStyle style);
    void Link(const MarkdownCommandRequest& request, bool remove);
    void Task();
    void Table(const MarkdownCommandRequest& request);
    void Delete(std::span<const MarkdownRange> ranges);
    std::optional<MarkdownRange> InlineBody(const MarkdownInline& node) const;
    MarkdownRange VisibleBody(std::size_t leaf) const
    {
        auto first = nodes[leaf].block.content.last, last = nodes[leaf].block.content.first;
        for (const auto& node : plan.syntax_->Inlines())
        {
            if (node.source.first < nodes[leaf].block.content.first || node.source.last > nodes[leaf].block.content.last)
                continue;
            if (node.kind != MarkdownInlineKind::Text && node.kind != MarkdownInlineKind::Code && node.kind != MarkdownInlineKind::Image &&
                ! (node.kind == MarkdownInlineKind::Link && node.firstChild == MarkdownNoNode))
                continue;
            const auto range = node.kind == MarkdownInlineKind::Code ? InlineBody(node).value_or(node.source) : node.source;
            first            = std::min(first, range.first);
            last             = std::max(last, range.last);
        }
        return first <= last ? MarkdownRange{first, last} : nodes[leaf].block.content;
    }
    bool InlineSelection(std::size_t& leaf) const
    {
        leaf = LeafAt(plan.before_.Start());
        if (leaf == MarkdownNoNode && plan.before_.anchor == plan.before_.caret && Line(plan.before_.caret).first == Line(plan.before_.caret).bodyEnd)
            return NodeAt(Kind::FencedCode, plan.before_.caret) == MarkdownNoNode && NodeAt(Kind::IndentedCode, plan.before_.caret) == MarkdownNoNode &&
                   NodeAt(Kind::HtmlBlock, plan.before_.caret) == MarkdownNoNode;
        return leaf != MarkdownNoNode && plan.before_.End() <= nodes[leaf].block.content.last;
    }
    bool EmptyStructuralSite() const
    {
        if (plan.before_.anchor != plan.before_.caret)
            return false;
        for (const auto kind : {Kind::FencedCode, Kind::IndentedCode, Kind::HtmlBlock, Kind::Table})
            if (NodeAt(kind, plan.before_.caret) != MarkdownNoNode)
                return false;
        const auto& line = Line(plan.before_.caret);
        if (line.first == line.bodyEnd)
            return true;
        for (const auto& empty : plan.syntax_->EmptyLines())
            if (empty.insertion == plan.before_.caret && empty.source.first == line.first)
                return true;
        return false;
    }
    // Code conversion owns the selected spelling. Decode existing text tokens
    // through the same source-unit resolver as rendering; never decode a code
    // token as Markdown or guess coordinates inside an entity.
    std::optional<std::wstring> CodeValue(std::size_t leaf, MarkdownRange selected) const
    {
        SemanticValue normalized, output;
        NormalizeSemanticLeaf(*plan.syntax_, leaf, normalized);
        for (const auto& unit : normalized.units)
            if ((unit.transformed || unit.generated) && ((unit.source.first < selected.first && selected.first < unit.source.last) ||
                                                         (unit.source.first < selected.last && selected.last < unit.source.last)))
                return std::nullopt;
        for (const auto& node : plan.syntax_->Inlines())
        {
            if (node.source.last <= selected.first || node.source.first >= selected.last)
                continue;
            if (node.source.first < nodes[leaf].block.source.first || node.source.last > nodes[leaf].block.source.last)
                continue;
            if (node.kind == MarkdownInlineKind::Html || node.kind == MarkdownInlineKind::Image)
                return std::nullopt;
            if (node.kind == MarkdownInlineKind::Text || node.kind == MarkdownInlineKind::Code)
            {
                SemanticValue value;
                ResolveSemanticValue(*plan.syntax_, normalized, node, value);
                SliceSemanticValue(value, selected, output);
            }
            else if (node.kind == MarkdownInlineKind::SoftBreak || node.kind == MarkdownInlineKind::HardBreak)
                return std::nullopt; // A single inline code command cannot own physical line prefixes/EOLs.
            else if (node.kind == MarkdownInlineKind::Link && node.firstChild == MarkdownNoNode)
                return std::nullopt;
        }
        return std::optional<std::wstring>(std::in_place, output.text);
    }
    bool HasOpaqueSelection() const
    {
        for (const auto& node : nodes)
            if (node.block.source.first < plan.before_.End() && node.block.source.last > plan.before_.Start() &&
                (node.block.kind == Kind::FencedCode || node.block.kind == Kind::IndentedCode || node.block.kind == Kind::HtmlBlock ||
                 node.block.kind == Kind::Table || node.block.kind == Kind::ThematicBreak))
                return true;
        return false;
    }
    std::optional<std::wstring> InlineSpelling(
        std::size_t leaf, MarkdownRange selected, std::wstring_view punctuation, bool stageEscapes = false, bool checkAdjacent = true)
    {
        SemanticValue normalized;
        NormalizeSemanticLeaf(*plan.syntax_, leaf, normalized);
        std::vector<std::size_t> escapes(0u);
        for (const auto& node : plan.syntax_->Inlines())
        {
            if (node.kind != MarkdownInlineKind::Text || node.source.first < nodes[leaf].block.source.first || node.source.last > nodes[leaf].block.source.last)
                continue;
            SemanticValue value;
            ResolveSemanticValue(*plan.syntax_, normalized, node, value);
            for (std::size_t i = 0u; i < value.units.size(); ++i)
            {
                const auto& unit = value.units[i];
                if ((unit.source.first < selected.first && selected.first < unit.source.last) ||
                    (unit.source.first < selected.last && selected.last < unit.source.last))
                    return std::nullopt;
                if (unit.generated || unit.transformed || unit.source.last - unit.source.first != 1u ||
                    punctuation.find(value.text[i]) == std::wstring_view::npos)
                    continue;
                // A neighboring literal delimiter cannot be changed outside the
                // command's ownership. Actual syntax delimiters are not Text
                // units and can still nest normally (e.g. italic inside bold).
                if (checkAdjacent && (unit.source.last == selected.first || unit.source.first == selected.last))
                    return std::nullopt;
                if (unit.source.first >= selected.first && unit.source.last <= selected.last)
                    escapes.push_back(unit.source.first);
            }
        }
        std::sort(escapes.begin(), escapes.end());
        std::wstring result(0u, L' ');
        auto cursor = selected.first;
        for (const auto at : escapes)
        {
            result += Slice(cursor, at);
            result += L'\\';
            cursor = at;
        }
        result += Slice(cursor, selected.last);
        if (stageEscapes)
            for (const auto at : escapes)
                Add(at, at, L"\\");
        return std::optional<std::wstring>(std::in_place, result);
    }
    std::size_t MapPosition(std::size_t position) const
    {
        std::size_t previous = 0u, mapped = 0u;
        for (const auto& edit : plan.edits_)
        {
            if (position < edit.source.first)
                return mapped + position - previous;
            mapped += edit.source.first - previous;
            if (position < edit.source.last)
                return mapped + edit.replacement.size();
            mapped += edit.replacement.size();
            previous = edit.source.last;
        }
        return mapped + position - previous;
    }
    MarkdownCommandPlan Finish()
    {
        if (! plan.applicable_)
        {
            plan.edits_.clear();
            return plan;
        }
        std::sort(plan.edits_.begin(), plan.edits_.end(), [](const auto& a, const auto& b) { return a.source.first < b.source.first; });
        for (std::size_t i = 1u; i < plan.edits_.size(); ++i)
        {
            if (plan.edits_[i].source.first < plan.edits_[i - 1u].source.last || plan.edits_[i].source.first == plan.edits_[i - 1u].source.first)
                throw std::logic_error("Overlapping structural command edits");
        }
        std::size_t removed = 0u, inserted = 0u;
        for (const auto& edit : plan.edits_)
        {
            removed += edit.source.last - edit.source.first;
            if (edit.replacement.size() > Document::MaximumUnits - inserted)
                throw std::length_error("Structural insertion capacity exceeded");
            inserted += edit.replacement.size();
        }
        if (inserted > Document::MaximumUnits - (text.size() - removed))
            throw std::length_error("Structural command source capacity exceeded");
        const auto length = text.size() - removed + inserted;
        if (! explicitSelection)
            plan.after_ = {MapPosition(plan.after_.anchor), MapPosition(plan.after_.caret)};
        std::wstring candidate(0u, L' ');
        candidate.reserve(length);
        std::size_t cursor = 0u;
        for (const auto& edit : plan.edits_)
        {
            candidate += Slice(cursor, edit.source.first);
            candidate += edit.replacement;
            cursor = edit.source.last;
        }
        candidate += Slice(cursor, text.size());
        if (! IsValidUtf16(candidate) || ! IsEditBoundary(candidate, plan.after_.anchor) || ! IsEditBoundary(candidate, plan.after_.caret))
            throw std::out_of_range("Invalid structural command result selection");
        return plan;
    }
};

void MarkdownCommandPlanner::SetList(const MarkdownCommandRequest& request)
{
    if (HasOpaqueSelection())
        return;
    if (request.listKind != MarkdownListKind::None && request.listKind != MarkdownListKind::Unordered && request.listKind != MarkdownListKind::Ordered &&
        request.listKind != MarkdownListKind::Task)
        throw std::invalid_argument("Invalid Markdown list kind");
    if (request.listStart && *request.listStart > 999999999u)
        throw std::out_of_range("Ordered list start exceeds nine digits");
    if (request.listKind == MarkdownListKind::Task && ! HasMarkdownExtension(plan.syntax_->Extensions(), MarkdownExtensions::TaskLists))
        return;
    const auto items = SelectedItems();
    auto number      = request.listStart.value_or(1u);
    if (! items.empty())
    {
        for (const auto item : items)
        {
            ItemPrefix prefix;
            if (! Prefix(item, prefix))
            {
                plan.edits_.clear();
                return;
            }
            if (request.listKind != MarkdownListKind::Task && ! TaskLabelCanUnwrap(item, prefix))
            {
                plan.edits_.clear();
                return;
            }
            if (request.listKind == MarkdownListKind::None)
            {
                if (! ShiftItem(item, -static_cast<int>(nodes[item].contentIndent), true))
                {
                    plan.edits_.clear();
                    return;
                }
                continue;
            }
            const auto symbol        = nodes[nodes[item].parent].listMarker;
            const bool ordered       = symbol == L'.' || symbol == L')';
            const bool wantedOrdered = request.listKind == MarkdownListKind::Ordered;
            if (ordered != wantedOrdered || (wantedOrdered && request.listStart))
            {
                const auto marker = ListMarker(wantedOrdered, number);
                if (wantedOrdered)
                    ++number;
                if (number > 1000000000u)
                    throw std::out_of_range("Ordered list numbering overflow");
                Add(prefix.first, prefix.content, marker);
                // Continuation indentation belongs to the affected item. Preserve
                // its content, sibling numbers and all physical line endings.
                const auto& line  = Line(nodes[item].block.source.first);
                const auto column = Columns(Slice(line.first, prefix.first));
                const auto delta  = static_cast<int>(Columns(marker, column)) - static_cast<int>(Columns(Slice(prefix.first, prefix.content), column));
                if (! ShiftFollowing(item, Line(nodes[item].block.source.first), delta))
                {
                    plan.edits_.clear();
                    return;
                }
            }
            if (request.listKind == MarkdownListKind::Task && nodes[item].task == MarkdownTask::None)
                Add(prefix.content, prefix.content, L"[ ] ");
            else if (request.listKind != MarkdownListKind::Task && nodes[item].task != MarkdownTask::None)
                Add(prefix.content, LabelStart(item, prefix), L"");
        }
        plan.applicable_ = true;
        return;
    }
    if (request.listKind == MarkdownListKind::None)
    {
        plan.applicable_ = true;
        return;
    }
    auto end = plan.before_.End();
    if (end > plan.before_.Start() && Line(end).first == end)
        --end;
    bool found = false;
    for (std::size_t i = 1u; i < nodes.size(); ++i)
    {
        const auto& node = nodes[i];
        if (node.block.source.last <= plan.before_.Start() || node.block.source.first > end)
            continue;
        if (! TextLeaf(node.block.kind))
            continue;
        if (Ancestor(i, Kind::ListItem) != MarkdownNoNode || node.block.kind == Kind::TableCell ||
            (request.listKind == MarkdownListKind::Task && node.block.kind != Kind::Paragraph))
        {
            plan.edits_.clear();
            return;
        }
        for (const auto& content : node.lines)
        {
            const auto& line = Line(content.source.first);
            if (line.end <= plan.before_.Start() || line.first > end)
                continue;
            std::size_t at{};
            if (! OuterOffset(i, line, at))
            {
                plan.edits_.clear();
                return;
            }
            while (at < line.bodyEnd && Space(text[at]))
                ++at;
            std::wstring marker = ListMarker(request.listKind == MarkdownListKind::Ordered, number);
            if (request.listKind == MarkdownListKind::Ordered)
                ++number;
            if (request.listKind == MarkdownListKind::Task)
                marker += L"[ ] ";
            Add(at, at, marker);
            found = true;
        }
    }
    if (! found && EmptyStructuralSite())
    {
        std::wstring marker = ListMarker(request.listKind == MarkdownListKind::Ordered, number);
        if (request.listKind == MarkdownListKind::Task)
            marker += L"[ ] ";
        Add(plan.before_.caret, plan.before_.caret, marker);
        found = true;
    }
    plan.applicable_ = found;
}
void MarkdownCommandPlanner::Task()
{
    if (! HasMarkdownExtension(plan.syntax_->Extensions(), MarkdownExtensions::TaskLists))
        return;
    const auto items = SelectedItems();
    if (items.size() > 1u)
    {
        if (std::ranges::any_of(items, [&](auto index) { return nodes[index].task == MarkdownTask::None; }))
        {
            MarkdownCommandRequest request;
            request.command  = MarkdownCommand::SetList;
            request.listKind = MarkdownListKind::Task;
            SetList(request);
            return;
        }
        for (const auto index : items)
        {
            ItemPrefix prefix;
            if (! Prefix(index, prefix))
                return;
            Add(prefix.content + 1u, prefix.content + 2u, nodes[index].task == MarkdownTask::Checked ? L" " : L"x");
        }
        plan.applicable_ = true;
        return;
    }
    const auto item = NodeAt(Kind::ListItem, plan.before_.caret);
    if (item == MarkdownNoNode || nodes[item].task == MarkdownTask::None)
    {
        MarkdownCommandRequest request;
        request.command  = MarkdownCommand::SetList;
        request.listKind = MarkdownListKind::Task;
        SetList(request);
        return;
    }
    ItemPrefix prefix;
    if (! Prefix(item, prefix))
        return;
    if (prefix.content + 3u > text.size() || text[prefix.content] != L'[' || text[prefix.content + 2u] != L']')
        return;
    Add(prefix.content + 1u, prefix.content + 2u, nodes[item].task == MarkdownTask::Checked ? L" " : L"x");
    plan.applicable_ = true;
}
void MarkdownCommandPlanner::Join(bool backwards)
{
    if (plan.before_.anchor != plan.before_.caret)
        return;
    const auto current = LeafAt(plan.before_.caret);
    if (current == MarkdownNoNode || nodes[current].block.kind != Kind::Paragraph || nodes[current].lines.empty())
        return;
    const auto visible = VisibleBody(current);
    if (backwards ? plan.before_.caret > visible.first : plan.before_.caret < visible.last)
        return;
    const auto item = Ancestor(current, Kind::ListItem);
    auto previous   = backwards ? PreviousSibling(current) : current;
    auto next       = backwards ? current : nodes[current].nextSibling;
    if (item != MarkdownNoNode)
    {
        if (nodes[item].firstChild != current || nodes[current].nextSibling != MarkdownNoNode)
            return;
        const auto other = backwards ? PreviousSibling(item) : nodes[item].nextSibling;
        if (other == MarkdownNoNode)
        {
            if (backwards)
                Indent(true);
            return;
        }
        const auto leaf = nodes[other].firstChild;
        if (leaf == MarkdownNoNode || nodes[leaf].block.kind != Kind::Paragraph || nodes[leaf].nextSibling != MarkdownNoNode)
            return;
        previous = backwards ? leaf : current;
        next     = backwards ? current : leaf;
    }
    if (previous == MarkdownNoNode || next == MarkdownNoNode || nodes[previous].block.kind != Kind::Paragraph || nodes[next].block.kind != Kind::Paragraph)
        return;
    const auto first = nodes[previous].block.content.last;
    auto last        = nodes[next].block.content.first;
    if (const auto nextItem = Ancestor(next, Kind::ListItem); nextItem != MarkdownNoNode)
    {
        ItemPrefix prefix;
        if (! Prefix(nextItem, prefix))
            return;
        last = LabelStart(nextItem, prefix);
    }
    if (first > last)
        return;
    // The entire interval is a proven paragraph boundary, including the next
    // item's own marker/task prefix. Opaque/nested subtrees were rejected above.
    Add(first, last, L" ");
    plan.after_       = {first, first};
    explicitSelection = true;
    plan.applicable_  = true;
}
std::optional<MarkdownRange> MarkdownCommandPlanner::InlineBody(const MarkdownInline& node) const
{
    auto first = node.source.first, last = node.source.last;
    if (node.kind == MarkdownInlineKind::Strong || node.kind == MarkdownInlineKind::Emphasis || node.kind == MarkdownInlineKind::Strikethrough)
    {
        const auto count = node.kind == MarkdownInlineKind::Emphasis ? 1u : 2u;
        if (last - first < count * 2u)
            return std::nullopt;
        return MarkdownRange{first + count, last - count};
    }
    if (node.kind == MarkdownInlineKind::Code)
    {
        while (first < last && text[first] == L'`')
            ++first;
        while (last > first && text[last - 1u] == L'`')
            --last;
        if (first < last && text[first] == L' ' && text[last - 1u] == L' ' && Slice(first, last).find_first_not_of(L' ') != std::wstring_view::npos)
        {
            ++first;
            --last;
        }
        return MarkdownRange{first, last};
    }
    if (node.kind != MarkdownInlineKind::Link)
        return std::nullopt;
    if (text[first] == L'<' && text[last - 1u] == L'>')
        return MarkdownRange{first + 1u, last - 1u};
    if (text[first] != L'[')
        return node.source; // GFM bare autolink.
    auto depth = 1u;
    for (auto at = first + 1u; at < last; ++at)
    {
        if (text[at] == L'\\' && at + 1u < last)
        {
            ++at;
            continue;
        }
        const auto opaque = std::lower_bound(opaqueInlines.begin(), opaqueInlines.end(), at, [](auto range, auto position) { return range.first < position; });
        if (opaque != opaqueInlines.end() && opaque->first == at)
        {
            at = opaque->last - 1u;
            continue;
        }
        if (text[at] == L'[')
            ++depth;
        if (text[at] == L']' && --depth == 0u)
            return MarkdownRange{first + 1u, at};
    }
    return std::nullopt;
}
namespace
{
std::wstring Styled(std::wstring_view value, MarkdownTextStyle style)
{
    if (value.empty())
        return std::wstring(0u, L' ');
    if (style == MarkdownTextStyle::Code)
    {
        std::size_t run = 0u, maximum = 0u;
        for (auto c : value)
        {
            run     = c == L'`' ? run + 1u : 0u;
            maximum = std::max(run, maximum);
        }
        const std::wstring fence(maximum + 1u, L'`');
        const bool pad = value.front() == L'`' || value.back() == L'`' ||
                         (value.front() == L' ' && value.back() == L' ' && value.find_first_not_of(L' ') != std::wstring_view::npos);
        std::wstring result(fence);
        if (pad)
            result += L' ';
        result += value;
        if (pad)
            result += L' ';
        result += fence;
        return std::wstring(result);
    }
    auto first = 0u;
    auto last  = value.size();
    while (first < last && (Space(value[first]) || value[first] == L'\r' || value[first] == L'\n'))
        ++first;
    while (last > first && (Space(value[last - 1u]) || value[last - 1u] == L'\r' || value[last - 1u] == L'\n'))
        --last;
    if (first == last)
        return std::wstring(value);
    const std::wstring_view marker = style == MarkdownTextStyle::Strong ? L"**" : style == MarkdownTextStyle::Emphasis ? L"*" : L"~~";
    std::wstring result(value.substr(0u, first));
    result += marker;
    result += value.substr(first, last - first);
    result += marker;
    result += value.substr(last);
    return std::wstring(result);
}
std::wstring LiteralMarkdown(std::wstring_view value)
{
    std::wstring result(0u, L' ');
    for (const auto c : value)
    {
        // Backslash escapes are defined for all ASCII punctuation. Escape even
        // currently harmless punctuation so a code literal cannot become a
        // heading, list, entity, HTML tag or link when its delimiters disappear.
        if ((c >= L'!' && c <= L'/') || (c >= L':' && c <= L'@') || (c >= L'[' && c <= L'`') || (c >= L'{' && c <= L'~'))
            result += L'\\';
        result += c;
    }
    return std::wstring(result);
}
std::wstring AttributeSource(std::wstring_view value, bool markdown = true)
{
    if (! IsValidUtf16(value))
        throw std::invalid_argument("Invalid Unicode link metadata");
    std::wstring result(0u, L' ');
    for (const auto c : value)
    {
        if (c < L' ' || (c >= 0x7f && c <= 0x9f))
            throw std::invalid_argument("Control character in link metadata");
        if (c == L'&')
            result += L"&amp;";
        else if (c == L'\"')
            result += L"&quot;";
        else if (c == L'\'')
            result += L"&#39;";
        else if (c == L'|')
            result += L"&#124;"; // Metadata must not manufacture GFM table separators.
        else if (c == L'\\' && markdown)
            result += L"\\\\";
        else
            result += c;
    }
    return std::wstring(result);
}
} // namespace
void MarkdownCommandPlanner::Style(MarkdownTextStyle style)
{
    if (style != MarkdownTextStyle::Strong && style != MarkdownTextStyle::Emphasis && style != MarkdownTextStyle::Strikethrough &&
        style != MarkdownTextStyle::Code)
        throw std::invalid_argument("Invalid Markdown text style");
    if (style == MarkdownTextStyle::Strikethrough && ! HasMarkdownExtension(plan.syntax_->Extensions(), MarkdownExtensions::Strikethrough))
        return;
    std::size_t leaf{};
    if (! InlineSelection(leaf))
        return;
    if (style == MarkdownTextStyle::Code && Line(plan.before_.Start()).first != Line(plan.before_.End()).first)
        return;
    const auto kind                 = style == MarkdownTextStyle::Strong          ? MarkdownInlineKind::Strong
                                      : style == MarkdownTextStyle::Emphasis      ? MarkdownInlineKind::Emphasis
                                      : style == MarkdownTextStyle::Strikethrough ? MarkdownInlineKind::Strikethrough
                                                                                  : MarkdownInlineKind::Code;
    const MarkdownInline* enclosing = nullptr;
    for (const auto& node : plan.syntax_->Inlines())
        if (node.kind == kind && node.source.first <= plan.before_.Start() && node.source.last >= plan.before_.End() &&
            (! enclosing || node.source.last - node.source.first < enclosing->source.last - enclosing->source.first))
            enclosing = &node;
    if (enclosing)
    {
        const auto body = InlineBody(*enclosing);
        if (! body)
            return;
        if (style == MarkdownTextStyle::Code)
        {
            SemanticValue normalized;
            NormalizeSemanticLeaf(*plan.syntax_, leaf, normalized);
            for (const auto& unit : normalized.units)
                if ((unit.source.first < plan.before_.Start() && plan.before_.Start() < unit.source.last) ||
                    (unit.source.first < plan.before_.End() && plan.before_.End() < unit.source.last))
                    return;
        }
        for (const auto& node : plan.syntax_->Inlines())
            if ((node.kind == MarkdownInlineKind::Html || node.kind == MarkdownInlineKind::Image) && node.source.first < body->last &&
                node.source.last > body->first)
                return;
        auto first = plan.before_.Start(), last = plan.before_.End();
        if (first == last || (first <= body->first && last >= body->last))
        {
            if (style == MarkdownTextStyle::Code)
            {
                SemanticValue normalized, value;
                NormalizeSemanticLeaf(*plan.syntax_, leaf, normalized);
                ResolveSemanticValue(*plan.syntax_, normalized, *enclosing, value);
                for (const auto& unit : value.units)
                    if ((unit.source.first < first && first < unit.source.last) || (unit.source.first < last && last < unit.source.last))
                        return;
                const auto replacement = LiteralMarkdown(value.text);
                Add(enclosing->source.first, enclosing->source.last, replacement);
                const auto begin = enclosing->source.first, end = begin + replacement.size();
                auto caret = begin;
                if (first == last)
                {
                    std::size_t count = 0u;
                    while (count < value.units.size() && value.units[count].source.last <= first)
                        ++count;
                    caret += LiteralMarkdown(std::wstring_view(value.text).substr(0u, count)).size();
                }
                plan.after_       = first == last                               ? Selection{caret, caret}
                                    : plan.before_.anchor <= plan.before_.caret ? Selection{begin, end}
                                                                                : Selection{end, begin};
                explicitSelection = true;
                plan.applicable_  = true;
                return;
            }
            // Removing delimiters can expose a literal heading/list/pipe/etc.
            // Escape literal Text units only; nested syntax and existing encoded
            // or escaped spelling remain owned by their original tokens.
            static_cast<void>(InlineSpelling(leaf, *body, L"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", true, false));
            Add(enclosing->source.first, body->first, L"");
            Add(body->last, enclosing->source.last, L"");
            plan.applicable_ = true;
            return;
        }
        if (first < body->first || last > body->last)
            return;
        if (style == MarkdownTextStyle::Code && Line(body->first).first != Line(body->last).first)
            return;
        for (const auto& node : plan.syntax_->Inlines())
            if (&node != enclosing && node.kind != MarkdownInlineKind::Text && node.source.first >= body->first && node.source.last <= body->last &&
                node.source.first < last && node.source.last > first && (first > node.source.first || last < node.source.last))
                return;
        const auto punctuation = style == MarkdownTextStyle::Strikethrough ? L"~" : L"*";
        const auto leftSource  = InlineSpelling(leaf, {body->first, first}, punctuation);
        const auto rightSource = InlineSpelling(leaf, {last, body->last}, punctuation);
        if (! leftSource || ! rightSource)
            return;
        const auto left = Styled(*leftSource, style), right = Styled(*rightSource, style);
        const auto middle = style == MarkdownTextStyle::Code ? LiteralMarkdown(Slice(first, last)) : std::wstring(Slice(first, last));
        std::wstring replacement(left);
        replacement += middle;
        replacement += right;
        Add(enclosing->source.first, enclosing->source.last, replacement);
        const auto begin = enclosing->source.first + left.size(), end = begin + middle.size();
        plan.after_       = plan.before_.anchor <= plan.before_.caret ? Selection{begin, end} : Selection{end, begin};
        explicitSelection = true;
        plan.applicable_  = true;
        return;
    }
    for (const auto& node : plan.syntax_->Inlines())
        if ((node.kind == MarkdownInlineKind::Html || node.kind == MarkdownInlineKind::Image) && node.source.first < plan.before_.End() &&
            node.source.last > plan.before_.Start())
            return;
    for (const auto& lexeme : plan.syntax_->Lexemes())
        if (lexeme.kind != MarkdownLexemeKind::Text && ((lexeme.source.first < plan.before_.Start() && plan.before_.Start() < lexeme.source.last) ||
                                                        (lexeme.source.first < plan.before_.End() && plan.before_.End() < lexeme.source.last)))
            return;
    for (const auto& node : plan.syntax_->Inlines())
        if (node.kind == MarkdownInlineKind::Code && node.source.first < plan.before_.End() && node.source.last > plan.before_.Start() &&
            (plan.before_.Start() > node.source.first || plan.before_.End() < node.source.last))
        {
            // Other styles can surround a complete code token but cannot edit
            // its literal interior while claiming a semantic formatting change.
            return;
        }
    const auto first = plan.before_.Start(), last = plan.before_.End();
    if (first == last)
    {
        const std::wstring_view marker = style == MarkdownTextStyle::Strong          ? L"**"
                                         : style == MarkdownTextStyle::Emphasis      ? L"*"
                                         : style == MarkdownTextStyle::Strikethrough ? L"~~"
                                                                                     : L"`";
        std::wstring replacement(marker);
        replacement += marker;
        Add(first, first, replacement);
        plan.after_ = {first + marker.size(), first + marker.size()};
    }
    else
    {
        const auto code = style == MarkdownTextStyle::Code ? CodeValue(leaf, {first, last})
                                                           : InlineSpelling(leaf, {first, last}, style == MarkdownTextStyle::Strikethrough ? L"~" : L"*");
        if (! code || code->empty())
            return;
        std::wstring payload(*code);
        if (style == MarkdownTextStyle::Code && nodes[leaf].block.kind == Kind::TableCell)
            for (auto at = payload.find(L'|'); at != std::wstring::npos; at = payload.find(L'|', at + 2u))
                payload.insert(at, 1u, L'\\');
        const auto value       = std::wstring_view(payload);
        const auto replacement = Styled(value, style);
        Add(first, last, replacement);
        // Locate the exact selected bytes inside the newly owned delimiters.
        // Leading/trailing whitespace remains outside non-code styles.
        std::size_t prefix = 0u, suffix = 0u;
        if (style == MarkdownTextStyle::Code)
        {
            while (prefix < replacement.size() && replacement[prefix] == L'`')
                ++prefix;
            suffix         = prefix;
            const bool pad = value.front() == L'`' || value.back() == L'`' ||
                             (value.front() == L' ' && value.back() == L' ' && value.find_first_not_of(L' ') != std::wstring_view::npos);
            if (pad)
            {
                ++prefix;
                ++suffix;
            }
        }
        else
        {
            const auto marker = style == MarkdownTextStyle::Emphasis ? 1u : 2u;
            if (! Space(value.front()) && value.front() != L'\r' && value.front() != L'\n')
                prefix = marker;
            if (! Space(value.back()) && value.back() != L'\r' && value.back() != L'\n')
                suffix = marker;
            if (replacement == value)
                prefix = suffix = 0u;
        }
        const auto begin = first + prefix, end = first + replacement.size() - suffix;
        plan.after_ = plan.before_.anchor <= plan.before_.caret ? Selection{begin, end} : Selection{end, begin};
    }
    explicitSelection = true;
    plan.applicable_  = true;
}
void MarkdownCommandPlanner::Link(const MarkdownCommandRequest& request, bool remove)
{
    if (! remove && (! NativeHtmlDestination(request.destination) || ! IsValidUtf16(request.destination)))
        throw std::invalid_argument("Ineligible inert link destination");
    auto leaf = LeafAt(plan.before_.Start());
    if (leaf == MarkdownNoNode)
        leaf = NodeAt(Kind::HtmlBlock, plan.before_.Start());
    if (leaf == MarkdownNoNode)
    {
        std::size_t unused{};
        if (remove || ! InlineSelection(unused))
            return;
        std::wstring replacement(L"[](<");
        replacement += AttributeSource(request.destination);
        replacement += L'>';
        if (! request.title.empty())
        {
            replacement += L" \"";
            replacement += AttributeSource(request.title);
            replacement += L'\"';
        }
        replacement += L")";
        const auto at = plan.before_.caret;
        Add(at, at, replacement);
        plan.after_       = {at + 1u, at + 1u};
        explicitSelection = true;
        plan.applicable_  = true;
        return;
    }
    if (plan.before_.End() > nodes[leaf].block.source.last)
        return;
    const MarkdownInline* enclosing = nullptr;
    for (const auto& node : plan.syntax_->Inlines())
        if (node.kind == MarkdownInlineKind::Link && node.source.first <= plan.before_.Start() && node.source.last >= plan.before_.End() &&
            (! enclosing || node.source.last - node.source.first < enclosing->source.last - enclosing->source.first))
            enclosing = &node;
    SemanticValue normalized;
    NormalizeSemanticLeaf(*plan.syntax_, leaf, normalized);
    NativeHtmlPlan html;
    if (nodes[leaf].block.kind == Kind::HtmlBlock)
        html.Block(normalized);
    else
        html.Inline(*plan.syntax_, nodes[leaf], normalized);
    if (html.present && ! html.valid)
        return;
    for (const auto& tag : html.tags)
    {
        if (tag.kind != NativeHtmlKind::A || tag.closing)
            continue;
        const auto& close = html.tags[tag.mate];
        if (tag.source.first <= plan.before_.Start() && close.source.last >= plan.before_.End())
        {
            if (remove)
            {
                if (nodes[leaf].block.kind == Kind::HtmlBlock && &tag == html.tags.data())
                {
                    // Removing the opening block anchor would reinterpret its
                    // literal body as Markdown. That block conversion is not an
                    // inline unlink: leave source exact and reject the target.
                    return;
                }
                Add(tag.source.first, tag.source.last, L"");
                Add(close.source.first, close.source.last, L"");
            }
            else
            {
                // This pass locates attributes of an already validated tag. Raw
                // source-unit mapping retains case, quote choice, order, spacing
                // and container prefixes, including tags split over CRLF.
                const auto unit = std::lower_bound(normalized.units.begin(), normalized.units.end(), tag.source.first, [](const auto& value, auto position) {
                    return value.source.first < position;
                });
                const auto base = static_cast<std::size_t>(unit - normalized.units.begin());
                auto at         = base + 1u;
                while (at < normalized.text.size() && ! HtmlSpace(normalized.text[at]) && normalized.text[at] != L'>')
                    ++at;
                bool title = false;
                while (at < normalized.text.size() && normalized.text[at] != L'>')
                {
                    while (at < normalized.text.size() && HtmlSpace(normalized.text[at]))
                        ++at;
                    if (at == normalized.text.size() || normalized.text[at] == L'>')
                        break;
                    const auto nameFirst = at;
                    while (at < normalized.text.size() && ! HtmlSpace(normalized.text[at]) && normalized.text[at] != L'=')
                        ++at;
                    const auto name = std::wstring_view(normalized.text).substr(nameFirst, at - nameFirst);
                    while (at < normalized.text.size() && normalized.text[at] != L'=')
                        ++at;
                    ++at;
                    while (at < normalized.text.size() && HtmlSpace(normalized.text[at]))
                        ++at;
                    const auto quote = normalized.text.at(at++);
                    const auto first = at;
                    while (at < normalized.text.size() && normalized.text[at] != quote)
                        ++at;
                    const auto last  = at++;
                    const auto range = [&](std::size_t begin, std::size_t end)
                    {
                        const auto start = normalized.units.at(begin).source.first;
                        return MarkdownRange{start, begin == end ? start : normalized.units.at(end - 1u).source.last};
                    };
                    if (HtmlName(name, L"href"))
                    {
                        const auto value = range(first, last);
                        Add(value.first, value.last, AttributeSource(request.destination, false));
                    }
                    else
                    {
                        title            = true;
                        const auto value = request.title.empty() ? range(nameFirst, at) : range(first, last);
                        Add(value.first, value.last, AttributeSource(request.title, false));
                    }
                }
                if (! title && ! request.title.empty())
                {
                    std::wstring attribute(L" title=\"");
                    attribute += AttributeSource(request.title, false);
                    attribute += L'\"';
                    Add(tag.source.last - 1u, tag.source.last - 1u, attribute);
                }
            }
            plan.applicable_ = true;
            return;
        }
        if (tag.source.first < plan.before_.End() && close.source.last > plan.before_.Start())
            return;
    }
    MarkdownRange owned{plan.before_.Start(), plan.before_.End()}, label = owned;
    if (enclosing)
    {
        const auto body = InlineBody(*enclosing);
        if (! body)
            return;
        owned = enclosing->source;
        label = *body;
    }
    else
    {
        if (remove)
            return;
        for (const auto& node : plan.syntax_->Inlines())
            if (node.kind == MarkdownInlineKind::Link && node.source.first < owned.last && node.source.last > owned.first)
                return;
            else if ((node.kind == MarkdownInlineKind::Code || node.kind == MarkdownInlineKind::Html || node.kind == MarkdownInlineKind::Image) &&
                     node.source.first < owned.last && node.source.last > owned.first && (owned.first > node.source.first || owned.last < node.source.last))
                return;
        if (nodes[leaf].block.kind == Kind::HtmlBlock)
            return;
    }
    std::wstring serialized(0u, L' ');
    {
        // The shared token resolver distinguishes literal autolink children
        // from decoded ordinary labels. Escape those literal URI punctuation
        // units when retargeting, and suppress automatic recognition on unlink.
        const bool autolink = enclosing && text[enclosing->source.first] != L'[';
        const auto spelling = InlineSpelling(leaf, label, (remove || autolink) ? L"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~" : L"[]", false, ! remove && ! autolink);
        if (! spelling)
            return;
        serialized = *spelling;
    }
    std::wstring replacement(serialized);
    std::size_t begin = owned.first;
    if (! remove)
    {
        std::wstring linked(L"[");
        linked += replacement;
        linked += L"](<";
        linked += AttributeSource(request.destination);
        linked += L'>';
        replacement = linked;
        if (! request.title.empty())
        {
            replacement += L" \"";
            replacement += AttributeSource(request.title);
            replacement += L'\"';
        }
        replacement += L")";
        ++begin;
    }
    Add(owned.first, owned.last, replacement);
    const auto end    = begin + serialized.size();
    plan.after_       = plan.before_.anchor <= plan.before_.caret ? Selection{begin, end} : Selection{end, begin};
    explicitSelection = true;
    plan.applicable_  = true;
}
void MarkdownCommandPlanner::Table(const MarkdownCommandRequest& request)
{
    if (! HasMarkdownExtension(plan.syntax_->Extensions(), MarkdownExtensions::Tables))
        return;
    const auto emptyRow = [](std::size_t columns, bool delimiter)
    {
        std::wstring value = L"|";
        for (std::size_t c = 0u; c < columns; ++c)
            value += delimiter ? L" --- |" : L" |";
        return std::wstring(value);
    };
    if (request.command == MarkdownCommand::InsertTable)
    {
        if (request.tableColumns == 0u || request.tableColumns > 64u || request.tableBodyRows > 256u ||
            (request.tableBodyRows + 1u) * request.tableColumns > 4096u)
            throw std::length_error("Structural table dimensions exceed command capacity");
        const auto& line = Line(plan.before_.Start());
        if (Line(plan.before_.End()).first != line.first)
            return;
        const auto leaf = LeafAt(plan.before_.Start());
        auto at         = line.first;
        if (leaf != MarkdownNoNode)
        {
            if (nodes[leaf].block.kind != Kind::Paragraph || nodes[leaf].lines.size() != 1u || ! OuterOffset(leaf, line, at))
                return;
            if (plan.before_.Start() > nodes[leaf].block.content.first || plan.before_.End() < nodes[leaf].block.content.last)
                return;
        }
        else
        {
            if (NodeAt(Kind::FencedCode, at) != MarkdownNoNode || NodeAt(Kind::IndentedCode, at) != MarkdownNoNode ||
                NodeAt(Kind::HtmlBlock, at) != MarkdownNoNode)
                return;
            while (at < line.bodyEnd && (Space(text[at]) || text[at] == L'>'))
                ++at;
            if (at != line.bodyEnd || plan.before_.Start() != plan.before_.End() || plan.before_.caret != at)
                return;
        }
        std::wstring prefix(Slice(line.first, at));
        // A selected list paragraph retains its opening marker outside the edit;
        // subsequent table lines use that item's continuation indentation.
        if (leaf != MarkdownNoNode)
        {
            const auto item = Ancestor(leaf, Kind::ListItem);
            if (item != MarkdownNoNode)
            {
                std::size_t outer{};
                if (! OuterOffset(item, line, outer))
                    return;
                prefix.assign(Slice(line.first, outer));
                prefix.append(nodes[item].contentIndent, L' ');
            }
        }
        std::wstring value = emptyRow(request.tableColumns, false);
        value += Eol(line);
        value += prefix;
        value += emptyRow(request.tableColumns, true);
        for (std::size_t row = 0u; row < request.tableBodyRows; ++row)
        {
            value += Eol(line);
            value += prefix;
            value += emptyRow(request.tableColumns, false);
        }
        Add(plan.before_.Start(), plan.before_.End(), value);
        plan.after_       = {plan.before_.Start() + 2u, plan.before_.Start() + 2u};
        explicitSelection = true;
        plan.applicable_  = true;
        return;
    }
    auto cell = request.targetCell;
    if (cell != MarkdownNoNode && (cell >= nodes.size() || nodes[cell].block.kind != Kind::TableCell))
        throw std::out_of_range("Invalid revision-local table cell");
    auto row   = cell == MarkdownNoNode ? NodeAt(Kind::TableRow, plan.before_.caret) : nodes[cell].parent;
    auto table = row == MarkdownNoNode ? NodeAt(Kind::Table, plan.before_.caret) : nodes[row].parent;
    if (table == MarkdownNoNode)
        return;
    std::vector<std::size_t> rows(0u);
    for (auto i = nodes[table].firstChild; i != MarkdownNoNode; i = nodes[i].nextSibling)
        rows.push_back(i);
    if (rows.empty())
        return;
    if (row == MarkdownNoNode)
        row = rows.front();
    if (plan.before_.Start() < nodes[row].block.source.first || plan.before_.End() > nodes[row].block.source.last)
        return;
    std::vector<std::size_t> cells(0u), rowCells(0u);
    for (const auto r : rows)
        for (auto i = nodes[r].firstChild; i != MarkdownNoNode; i = nodes[i].nextSibling)
        {
            cells.push_back(i);
            if (r == row)
                rowCells.push_back(i);
        }
    const auto columns = cells.size() / rows.size();
    if (columns == 0u || columns > 64u || rows.size() > 257u || cells.size() > 4096u)
        return;
    if (cell == MarkdownNoNode)
    {
        cell = rowCells.front();
        for (const auto i : rowCells)
        {
            const auto range = nodes[i].block.content;
            if (range.first <= plan.before_.caret && (plan.before_.caret <= range.last))
            {
                cell = i;
                break;
            }
            if (range.first <= plan.before_.caret)
                cell = i;
        }
    }
    const auto column   = static_cast<std::size_t>(std::find(rowCells.begin(), rowCells.end(), cell) - rowCells.begin());
    const auto rowIndex = static_cast<std::size_t>(std::find(rows.begin(), rows.end(), row) - rows.begin());
    if (column >= columns || rowIndex >= rows.size())
        return;
    const auto& separator = Line(Line(nodes[rows.front()].block.source.first).end);
    auto separatorFirst   = separator.first;
    for (auto at = separator.first; at < separator.bodyEnd; ++at)
        if (text[at] == L'>')
            separatorFirst = at + 1u;
    while (separatorFirst < separator.bodyEnd && Space(text[separatorFirst]))
        ++separatorFirst;
    const MarkdownRange separatorRange{separatorFirst, separator.bodyEnd};
    const auto prefix          = Slice(separator.first, separatorFirst);
    const auto rawCellPosition = [&](std::size_t target)
    {
        const auto owner = nodes[target].parent;
        const auto range = nodes[target].block.content;
        return std::min(range.first, Line(nodes[owner].block.source.first).bodyEnd);
    };
    const auto appendCells = [&](MarkdownRange range, std::size_t target, std::wstring_view value)
    {
        const auto slots    = MarkdownTableCells(text, range, false);
        const auto at       = slots.back().last;
        const bool terminal = at < text.size() && text[at] == L'|';
        std::wstring tail(0u, L' ');
        std::size_t caret{};
        for (auto c = slots.size(); c <= target; ++c)
        {
            tail += terminal ? L"| " : L" | ";
            if (c == target)
            {
                caret = tail.size();
                tail += value;
            }
            if (terminal)
                tail += L' ';
        }
        Add(at, at, tail);
        return at + caret + value.size();
    };
    const auto insertRow = [&](std::size_t afterRow)
    {
        if (rows.size() == 257u || (rows.size() + 1u) * columns > 4096u)
            throw std::length_error("Structural table row capacity exceeded");
        const auto& physical = afterRow == rows.front() ? separator : Line(nodes[afterRow].block.source.first);
        const auto at        = physical.end;
        std::wstring value(0u, L' ');
        if (physical.end == physical.bodyEnd)
            value += Eol(physical);
        value += prefix;
        const auto caret = at + value.size() + 2u;
        value += emptyRow(columns, false);
        if (physical.end > physical.bodyEnd)
            value += Eol(physical);
        Add(at, at, value);
        plan.after_       = {caret, caret};
        explicitSelection = true;
    };
    switch (request.command)
    {
        case MarkdownCommand::NextTableCell:
        case MarkdownCommand::PreviousTableCell:
        {
            if (plan.before_.anchor != plan.before_.caret)
                return;
            const auto index = static_cast<std::size_t>(std::find(cells.begin(), cells.end(), cell) - cells.begin());
            if (request.command == MarkdownCommand::PreviousTableCell && index == 0u)
                return;
            if (request.command == MarkdownCommand::NextTableCell && index + 1u == cells.size())
                insertRow(row);
            else
            {
                const auto target = cells[request.command == MarkdownCommand::NextTableCell ? index + 1u : index - 1u];
                const auto at     = rawCellPosition(target);
                plan.after_       = {at, at};
                plan.afterCell_   = target;
                explicitSelection = true;
            }
            break;
        }
        case MarkdownCommand::SetTableCell:
        {
            if (! IsValidUtf16(request.text) || request.text.find_first_of(L"\r\n") != std::wstring_view::npos)
                throw std::invalid_argument("Cell source must be valid single-line Unicode");
            for (std::size_t at = 0u; at < request.text.size(); ++at)
            {
                if (request.text[at] == L'\\')
                {
                    if (at + 1u == request.text.size())
                        throw std::invalid_argument("Dangling table cell escape");
                    ++at;
                }
                else if (request.text[at] == L'|')
                    throw std::invalid_argument("Unescaped table cell separator");
            }
            const auto slots = MarkdownTableCells(text, nodes[row].block.content, false);
            std::size_t caret{};
            if (column >= slots.size())
                caret = appendCells(nodes[row].block.content, column, request.text);
            else
            {
                const auto range = nodes[cell].block.content;
                Add(range.first, range.last, request.text);
                caret = range.first + request.text.size();
            }
            plan.after_       = {caret, caret};
            explicitSelection = true;
            break;
        }
        case MarkdownCommand::InsertTableRow: insertRow(row); break;
        case MarkdownCommand::DeleteTableRow:
        {
            if (row == rows.front())
            {
                if (rows.size() == 1u)
                {
                    Add(nodes[table].block.source.first, nodes[table].block.source.last, L"");
                    plan.after_       = {nodes[table].block.source.first, nodes[table].block.source.first};
                    explicitSelection = true;
                    break;
                }
                const auto promote = rows[1u];
                auto value         = std::wstring(Slice(nodes[promote].block.content.first, nodes[promote].block.source.last));
                const auto actual  = MarkdownTableCells(text, nodes[promote].block.content, false);
                if (actual.size() > 64u || actual.size() * (rows.size() - 1u) > 4096u)
                    throw std::length_error("Promoted table header exceeds command capacity");
                if (actual.size() < columns)
                {
                    const auto at       = actual.back().last - nodes[promote].block.content.first;
                    const bool terminal = actual.back().last < text.size() && text[actual.back().last] == L'|';
                    std::wstring tail(0u, L' ');
                    for (auto c = actual.size(); c < columns; ++c)
                        tail += terminal ? L"|  " : L" | ";
                    value.insert(at, tail);
                }
                else if (actual.size() > columns)
                {
                    const auto slots    = MarkdownTableCells(text, separatorRange, false);
                    const auto at       = slots.back().last;
                    const bool terminal = at < text.size() && text[at] == L'|';
                    std::wstring tail(0u, L' ');
                    for (auto c = columns; c < actual.size(); ++c)
                        tail += terminal ? L"| --- " : L" | ---";
                    Add(at, at, tail);
                }
                Add(nodes[row].block.content.first, nodes[row].block.source.last, value);
                Add(nodes[promote].block.source.first, nodes[promote].block.source.last, L"");
                const auto at     = nodes[row].block.content.first + 2u;
                plan.after_       = {at, at};
                explicitSelection = true;
            }
            else
            {
                Add(nodes[row].block.source.first, nodes[row].block.source.last, L"");
                const auto targetRow = rows[rowIndex + 1u < rows.size() ? rowIndex + 1u : rowIndex - 1u];
                auto target          = nodes[targetRow].firstChild;
                for (std::size_t c = 0u; c < column; ++c)
                    target = nodes[target].nextSibling;
                const auto at = rawCellPosition(target);
                plan.after_   = {at, at};
            }
            break;
        }
        case MarkdownCommand::InsertTableColumn:
        case MarkdownCommand::DeleteTableColumn:
        {
            const bool insert = request.command == MarkdownCommand::InsertTableColumn;
            if (insert && (columns == 64u || (columns + 1u) * rows.size() > 4096u))
                throw std::length_error("Structural table column capacity exceeded");
            if (! insert && columns == 1u)
            {
                Add(nodes[table].block.source.first, nodes[table].block.source.last, L"");
                plan.after_       = {nodes[table].block.source.first, nodes[table].block.source.first};
                explicitSelection = true;
                break;
            }
            std::vector<MarkdownRange> ranges(0u);
            ranges.push_back(nodes[rows.front()].block.content);
            ranges.push_back(separatorRange);
            for (std::size_t r = 1u; r < rows.size(); ++r)
                ranges.push_back(nodes[rows[r]].block.content);
            for (std::size_t r = 0u; r < ranges.size(); ++r)
            {
                const auto slots = MarkdownTableCells(text, ranges[r], false);
                if (insert)
                {
                    if (column < slots.size())
                        Add(slots[column].first, slots[column].first, r == 1u ? L" --- |" : L"  |");
                    else
                        static_cast<void>(appendCells(ranges[r], column, r == 1u ? L"---" : L""));
                    if (r == 0u)
                    {
                        plan.after_       = {slots[column].first + 1u, slots[column].first + 1u};
                        explicitSelection = true;
                    }
                }
                else if (column < slots.size())
                {
                    if (column + 1u < slots.size())
                        Add(slots[column].first, slots[column].last + 1u, L"");
                    else
                        Add(slots[column - 1u].last, slots[column].last, L"");
                }
            }
            if (! insert)
            {
                auto target       = nodes[rows.front()].firstChild;
                const auto wanted = column + 1u < columns ? column + 1u : column - 1u;
                for (std::size_t c = 0u; c < wanted; ++c)
                    target = nodes[target].nextSibling;
                const auto at = rawCellPosition(target);
                plan.after_   = {at, at};
            }
            break;
        }
        default: return;
    }
    plan.applicable_ = true;
}
void MarkdownCommandPlanner::Delete(std::span<const MarkdownRange> ranges)
{
    std::vector<MarkdownRange> owned(ranges.begin(), ranges.end());
    for (const auto range : owned)
        if (range.first > range.last || ! plan.source_.IsBoundary(range.first) || ! plan.source_.IsBoundary(range.last))
            throw std::out_of_range("Invalid visual source deletion");
    const auto merge = [&]
    {
        std::sort(owned.begin(), owned.end(), [](auto a, auto b) { return a.first < b.first; });
        std::size_t count = 0u;
        for (const auto range : owned)
        {
            if (range.first == range.last)
                continue;
            if (count && range.first <= owned[count - 1u].last)
                owned[count - 1u].last = std::max(range.last, owned[count - 1u].last);
            else
                owned[count++] = range;
        }
        owned.resize(count);
    };
    struct Wrapper
    {
        MarkdownRange source, body;
    };
    std::vector<Wrapper> wrappers(0u);
    for (const auto& node : plan.syntax_->Inlines())
        if (const auto body = InlineBody(node); body && body->first < body->last)
            wrappers.push_back({node.source, *body});
    for (std::size_t i = 1u; i < nodes.size(); ++i)
    {
        if (! TextLeaf(nodes[i].block.kind) && nodes[i].block.kind != Kind::HtmlBlock)
            continue;
        SemanticValue normalized;
        NormalizeSemanticLeaf(*plan.syntax_, i, normalized);
        NativeHtmlPlan html;
        if (nodes[i].block.kind == Kind::HtmlBlock)
            html.Block(normalized);
        else
            html.Inline(*plan.syntax_, nodes[i], normalized);
        if (! html.valid)
            continue;
        for (const auto& tag : html.tags)
            if (! tag.closing && tag.mate != MarkdownNoNode && ! HtmlBlockWrapper(tag.kind))
                wrappers.push_back({{tag.source.first, html.tags[tag.mate].source.last}, {tag.source.last, html.tags[tag.mate].source.first}});
    }
    // Process inner ownership before outer ownership so ordinary nested wrappers
    // close in one pass. The fixed point also handles crossing native/Markdown
    // boundaries without making source-order assumptions.
    std::sort(wrappers.begin(), wrappers.end(), [](const auto& a, const auto& b) { return a.body.last - a.body.first < b.body.last - b.body.first; });
    merge();
    bool changed = true;
    while (changed)
    {
        changed = false;
        std::vector<MarkdownRange> expanded(0u);
        for (const auto& wrapper : wrappers)
        {
            const auto next = std::upper_bound(owned.begin(), owned.end(), wrapper.body.first, [](auto at, auto range) { return at < range.first; });
            if (next == owned.begin())
                continue;
            const auto& range = *std::prev(next);
            if (range.last >= wrapper.body.last && (range.first > wrapper.source.first || range.last < wrapper.source.last))
                expanded.push_back(wrapper.source);
        }
        changed = ! expanded.empty();
        owned.insert(owned.end(), expanded.begin(), expanded.end());
        if (changed)
            merge();
    }
    for (const auto range : owned)
        Add(range.first, range.last, L"");
    if (! owned.empty())
    {
        plan.after_       = {owned.front().first, owned.front().first};
        explicitSelection = true;
        plan.applicable_  = true;
    }
}
} // namespace Detail

MarkdownCommandPlan BuildMarkdownCommand(const SourceSnapshot& source,
                                         std::uint64_t revision,
                                         std::shared_ptr<const MarkdownSyntax> syntax,
                                         Selection selection,
                                         const MarkdownCommandRequest& request)
{
    if (request.text.size() > Document::MaximumUnits || request.destination.size() > Document::MaximumUnits || request.title.size() > Document::MaximumUnits)
        throw std::length_error("Structural request text exceeds source capacity");
    Detail::MarkdownCommandPlanner builder(source, revision, std::move(syntax), selection);
    switch (request.command)
    {
        case MarkdownCommand::InsertParagraph: builder.Continue(); break;
        case MarkdownCommand::SetList: builder.SetList(request); break;
        case MarkdownCommand::IndentList: builder.Indent(false); break;
        case MarkdownCommand::OutdentList: builder.Indent(true); break;
        case MarkdownCommand::JoinBackward: builder.Join(true); break;
        case MarkdownCommand::JoinForward: builder.Join(false); break;
        case MarkdownCommand::SetHeading: builder.Heading(request.headingLevel); break;
        case MarkdownCommand::ToggleStyle: builder.Style(request.style); break;
        case MarkdownCommand::SetLink: builder.Link(request, false); break;
        case MarkdownCommand::RemoveLink: builder.Link(request, true); break;
        case MarkdownCommand::ToggleTask: builder.Task(); break;
        case MarkdownCommand::InsertTable:
        case MarkdownCommand::InsertTableRow:
        case MarkdownCommand::DeleteTableRow:
        case MarkdownCommand::InsertTableColumn:
        case MarkdownCommand::DeleteTableColumn:
        case MarkdownCommand::SetTableCell:
        case MarkdownCommand::NextTableCell:
        case MarkdownCommand::PreviousTableCell: builder.Table(request); break;
        default: throw std::invalid_argument("Invalid Markdown command");
    }
    return builder.Finish();
}
MarkdownCommandPlan BuildMarkdownDeletion(const SourceSnapshot& source,
                                          std::uint64_t revision,
                                          std::shared_ptr<const MarkdownSyntax> syntax,
                                          Selection selection,
                                          std::span<const MarkdownRange> ranges)
{
    if (ranges.size() > SourceDisplayMap::MaximumSpans)
        throw std::length_error("Structural deletion fragment capacity exceeded");
    Detail::MarkdownCommandPlanner builder(source, revision, std::move(syntax), selection);
    builder.Delete(ranges);
    return builder.Finish();
}
bool ApplyMarkdownCommand(Document& document, const MarkdownCommandPlan& plan)
{
    if (document.Revision() != plan.ExpectedRevision() || ! document.Snapshot().IsSameRevision(plan.ExpectedSource()) ||
        document.GetSelection() != plan.Before())
        throw std::invalid_argument("Stale Markdown command source or selection");
    if (! plan.IsApplicable())
        return false;
    std::vector<SourceEdit> edits(0u);
    edits.reserve(plan.Edits().size());
    for (const auto& edit : plan.Edits())
        edits.push_back({edit.source, edit.replacement});
    return ApplySourceEdits(document, plan.ExpectedSource(), plan.ExpectedRevision(), edits, plan.After());
}
} // namespace DxUi::RichText
