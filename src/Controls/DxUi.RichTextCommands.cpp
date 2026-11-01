#include "DxUi.MarkdownTableCells.h"
#include "DxUi.RichFileReferenceContext.h"
#include "DxUi.RichImagePaths.h"
#include "DxUi.RichSemanticHtml.h"
#include <DxUi/RichMath.h>
#include <DxUi/RichTextCommands.h>
#include <limits>
#include <stdexcept>

namespace DxUi::RichText
{
namespace
{
size_t CommandNodeAt(const MarkdownSyntax& syntax, MarkdownBlockKind kind, size_t at) noexcept
{
    const auto& nodes = syntax.Nodes();
    const auto text   = syntax.Source();
    size_t best       = MarkdownNoNode;
    for (size_t i = 1u; i < nodes.size(); ++i)
    {
        const auto range = nodes[i].block.source;
        if (nodes[i].block.kind == kind && range.first <= at &&
            (at < range.last || (at == range.last && at == text.size() &&
                                 (text.empty() || (text.back() != L'\r' && text.back() != L'\n') ||
                                  ((kind == MarkdownBlockKind::FencedCode || kind == MarkdownBlockKind::FrontMatter) && ! nodes[i].block.closed)))) &&
            (best == MarkdownNoNode || range.last - range.first <= nodes[best].block.source.last - nodes[best].block.source.first))
            best = i;
    }
    return best;
}
struct RuleAdmission
{
    std::wstring_view eol;
    unsigned before{}, after{};
};
std::optional<RuleAdmission> InspectThematicBreak(const MarkdownSyntax& syntax, Selection selection) noexcept
{
    const auto text = syntax.Source();
    if (syntax.SourceRegion() || selection.Start() != selection.End() || ! IsEditBoundary(text, selection.caret))
        return std::nullopt;
    const auto caret  = selection.caret;
    const auto& nodes = syntax.Nodes();
    if (CommandNodeAt(syntax, MarkdownBlockKind::FootnoteDefinition, caret) != MarkdownNoNode)
        return std::nullopt;
    size_t leaf = MarkdownNoNode;
    for (const auto kind : {MarkdownBlockKind::Paragraph, MarkdownBlockKind::AtxHeading, MarkdownBlockKind::SetextHeading, MarkdownBlockKind::TableCell})
    {
        const auto node = CommandNodeAt(syntax, kind, caret);
        if (node != MarkdownNoNode && (leaf == MarkdownNoNode || nodes[node].block.source.last - nodes[node].block.source.first <
                                                                     nodes[leaf].block.source.last - nodes[leaf].block.source.first))
            leaf = node;
    }
    if (leaf != MarkdownNoNode && (nodes[leaf].block.kind != MarkdownBlockKind::Paragraph || nodes[leaf].parent == MarkdownNoNode ||
                                   nodes[nodes[leaf].parent].block.kind != MarkdownBlockKind::Document))
        return std::nullopt;
    size_t lineFirst = caret;
    while (lineFirst && text[lineFirst - 1u] != L'\r' && text[lineFirst - 1u] != L'\n')
        --lineFirst;
    auto lineEnd = caret;
    while (lineEnd < text.size() && text[lineEnd] != L'\r' && text[lineEnd] != L'\n')
        ++lineEnd;
    if (leaf == MarkdownNoNode)
    {
        for (const auto kind : {MarkdownBlockKind::FencedCode,
                                MarkdownBlockKind::IndentedCode,
                                MarkdownBlockKind::HtmlBlock,
                                MarkdownBlockKind::FrontMatter,
                                MarkdownBlockKind::Table})
            if (CommandNodeAt(syntax, kind, caret) != MarkdownNoNode)
                return std::nullopt;
        if (lineFirst != lineEnd &&
            std::ranges::none_of(syntax.EmptyLines(), [&](const auto& line) { return line.insertion == caret && line.source.first == lineFirst; }))
            return std::nullopt;
    }
    if (CommandNodeAt(syntax, MarkdownBlockKind::BlockQuote, caret) != MarkdownNoNode ||
        CommandNodeAt(syntax, MarkdownBlockKind::ListItem, caret) != MarkdownNoNode)
        return std::nullopt;
    for (const auto& node : syntax.Inlines())
        if (node.kind != MarkdownInlineKind::Text && node.source.first < caret && caret < node.source.last)
            return std::nullopt;
    auto eolAt      = lineEnd < text.size() ? lineEnd : text.find_first_of(L"\r\n");
    const auto eol  = eolAt == text.npos                                                              ? std::wstring_view(L"\r\n")
                      : text[eolAt] == L'\r' && eolAt + 1u < text.size() && text[eolAt + 1u] == L'\n' ? text.substr(eolAt, 2u)
                                                                                                      : text.substr(eolAt, 1u);
    unsigned before = 0u, after = 0u;
    for (auto at = caret; at > 0u && before < 2u; ++before)
    {
        if (text[at - 1u] == L'\n')
        {
            --at;
            if (at && text[at - 1u] == L'\r')
                --at;
        }
        else if (text[at - 1u] == L'\r')
            --at;
        else
            break;
    }
    for (auto at = caret; at < text.size() && after < 2u; ++after)
    {
        if (text[at] == L'\r')
        {
            ++at;
            if (at < text.size() && text[at] == L'\n')
                ++at;
        }
        else if (text[at] == L'\n')
            ++at;
        else
            break;
    }
    const unsigned prefix = caret == 0u ? 0u : 2u - before;
    const unsigned suffix = caret == text.size() ? 0u : 2u - after;
    if (3u + (prefix + suffix) * eol.size() > Document::MaximumUnits - text.size())
        return std::nullopt;
    return RuleAdmission{eol, prefix, suffix};
}
} // namespace
std::optional<std::size_t> FindCommandFootnoteOwner(const MarkdownSyntax& syntax, Selection selection) noexcept
{
    const auto& nodes = syntax.Nodes();
    auto note = MarkdownNoNode;
    for (size_t index = 0u; index < nodes.size(); ++index)
    {
        const auto& node = nodes[index];
        if (node.block.kind != MarkdownBlockKind::FootnoteDefinition ||
            ! (selection.Start() == selection.End() ? CommandNodeAt(syntax, MarkdownBlockKind::FootnoteDefinition, selection.Start()) == index
                                                     : node.block.source.first < selection.End() && selection.Start() < node.block.source.last))
            continue;
        const auto resolved = std::ranges::find(syntax.Footnotes(), index, &MarkdownFootnote::definition);
        if (note != MarkdownNoNode || resolved == syntax.Footnotes().end() || ! resolved->ordinal ||
            selection.Start() < node.block.content.first || selection.End() > node.block.source.last)
            return std::nullopt;
        note = index;
    }
    return note;
}
bool CanInsertThematicBreak(const MarkdownSyntax& syntax, Selection selection) noexcept
{
    return InspectThematicBreak(syntax, selection).has_value();
}
bool IsCommandFeatureEnabled(const MarkdownSyntax& syntax, MarkdownCommand command, MarkdownTextStyle style, MarkdownListKind listKind) noexcept
{
    const auto extensions = syntax.Extensions();
    if (command == MarkdownCommand::ToggleTask || (command == MarkdownCommand::SetList && listKind == MarkdownListKind::Task))
        return HasMarkdownExtension(extensions, MarkdownExtensions::TaskLists);
    if (command == MarkdownCommand::ToggleStyle && style == MarkdownTextStyle::Strikethrough)
        return HasMarkdownExtension(extensions, MarkdownExtensions::Strikethrough);
    if (command >= MarkdownCommand::InsertTable && command <= MarkdownCommand::PreviousTableCell)
        return HasMarkdownExtension(extensions, MarkdownExtensions::Tables);
    return true;
}
bool IsSingleLineCommandSelection(const MarkdownSyntax& syntax, Selection selection) noexcept
{
    const auto source = syntax.Source();
    const auto first = selection.Start(), last = selection.End();
    return last <= source.size() && source.substr(first, last - first).find_first_of(L"\r\n") == std::wstring_view::npos;
}
static MarkdownRange JoinVisibleBody(const MarkdownSyntax& syntax, std::size_t leaf) noexcept
{
    const auto& block = syntax.Nodes()[leaf].block.content;
    auto first = block.last, last = block.first;
    const auto text = syntax.Source();
    for (const auto& node : syntax.Inlines())
    {
        if (node.source.first < block.first || node.source.last > block.last)
            continue;
        if (node.kind != MarkdownInlineKind::Text && node.kind != MarkdownInlineKind::Code && node.kind != MarkdownInlineKind::Image &&
            ! (node.kind == MarkdownInlineKind::Link && node.firstChild == MarkdownNoNode))
            continue;
        auto range = node.source;
        if (node.kind == MarkdownInlineKind::Code)
        {
            while (range.first < range.last && text[range.first] == L'`')
                ++range.first;
            while (range.last > range.first && text[range.last - 1u] == L'`')
                --range.last;
            if (range.first < range.last && text[range.first] == L' ' && text[range.last - 1u] == L' ' &&
                text.substr(range.first, range.last - range.first).find_first_not_of(L' ') != std::wstring_view::npos)
            {
                ++range.first;
                --range.last;
            }
        }
        first = std::min(first, range.first);
        last  = std::max(last, range.last);
    }
    return first <= last ? MarkdownRange{first, last} : block;
}
std::optional<std::size_t> FindJoinLeaf(const MarkdownSyntax& syntax, Selection selection, bool backwards, bool* simpleApplicable) noexcept
{
    if (simpleApplicable)
        *simpleApplicable = false;
    if (! CanJoinSelection(selection))
        return std::nullopt;
    const auto& nodes = syntax.Nodes();
    auto best         = MarkdownNoNode;
    for (const auto kind : {MarkdownBlockKind::Paragraph, MarkdownBlockKind::AtxHeading, MarkdownBlockKind::SetextHeading, MarkdownBlockKind::TableCell})
    {
        const auto node = CommandNodeAt(syntax, kind, selection.caret);
        if (node != MarkdownNoNode && (best == MarkdownNoNode || nodes[node].block.source.last - nodes[node].block.source.first <
                                                                     nodes[best].block.source.last - nodes[best].block.source.first))
            best = node;
    }
    if (best == MarkdownNoNode || nodes[best].lines.empty())
        return std::nullopt;
    const auto kind = nodes[best].block.kind;
    if (kind != MarkdownBlockKind::Paragraph && ! (backwards && (kind == MarkdownBlockKind::AtxHeading || kind == MarkdownBlockKind::SetextHeading)))
        return std::nullopt;
    const auto visible = JoinVisibleBody(syntax, best);
    if (backwards ? selection.caret > visible.first : selection.caret < visible.last)
        return std::nullopt;
    if (kind != MarkdownBlockKind::Paragraph && simpleApplicable && nodes[best].parent != MarkdownNoNode &&
        nodes[nodes[best].parent].block.kind == MarkdownBlockKind::Document)
        *simpleApplicable = true;
    if (kind == MarkdownBlockKind::Paragraph)
    {
        auto nearest = nodes[best].parent;
        while (nearest != MarkdownNoNode && nodes[nearest].block.kind != MarkdownBlockKind::ListItem &&
               nodes[nearest].block.kind != MarkdownBlockKind::BlockQuote)
            nearest = nodes[nearest].parent;
        const bool quoteOutdent =
            backwards && nearest != MarkdownNoNode && nodes[nearest].block.kind == MarkdownBlockKind::BlockQuote && nodes[nearest].firstChild == best;
        // List joins and quote outdents need additional prefix admission. A
        // plain paragraph can reject missing or non-paragraph siblings here.
        bool insideList = false;
        for (auto parent = nodes[best].parent; parent != MarkdownNoNode; parent = nodes[parent].parent)
            insideList |= nodes[parent].block.kind == MarkdownBlockKind::ListItem;
        if (! insideList && ! quoteOutdent)
        {
            auto previous     = MarkdownNoNode;
            const auto parent = nodes[best].parent;
            if (parent != MarkdownNoNode)
                for (auto sibling = nodes[parent].firstChild; sibling != MarkdownNoNode && sibling != best; sibling = nodes[sibling].nextSibling)
                    previous = sibling;
            const auto left  = backwards ? previous : best;
            const auto right = backwards ? best : nodes[best].nextSibling;
            if (left == MarkdownNoNode || right == MarkdownNoNode || nodes[left].block.kind != MarkdownBlockKind::Paragraph ||
                nodes[right].block.kind != MarkdownBlockKind::Paragraph || nodes[left].block.content.last > nodes[right].block.content.first)
                return std::nullopt;
            const auto removed = nodes[right].block.content.first - nodes[left].block.content.last;
            if (simpleApplicable && parent != MarkdownNoNode && nodes[parent].block.kind == MarkdownBlockKind::Document &&
                syntax.Source().size() - removed < Document::MaximumUnits)
                *simpleApplicable = true;
        }
    }
    return best;
}
static std::optional<size_t> FindInlineInsertionSite(const MarkdownSyntax& syntax, Selection selection) noexcept
{
    const auto text  = syntax.Source();
    const auto first = selection.Start(), last = selection.End();
    if (syntax.SourceRegion() || ! IsEditBoundary(text, first) || ! IsEditBoundary(text, last))
        return std::nullopt;
    const auto& nodes = syntax.Nodes();
    const auto nodeAt = [&](MarkdownBlockKind kind, size_t at) noexcept { return CommandNodeAt(syntax, kind, at); };
    if (first == last)
    {
        if (nodeAt(MarkdownBlockKind::FootnoteDefinition, first) != MarkdownNoNode)
            return std::nullopt;
    }
    else
        for (const auto& node : nodes)
            if (node.block.kind == MarkdownBlockKind::FootnoteDefinition && node.block.source.first < last && first < node.block.source.last)
                return std::nullopt;
    size_t leaf = MarkdownNoNode;
    for (const auto kind : {MarkdownBlockKind::Paragraph, MarkdownBlockKind::AtxHeading, MarkdownBlockKind::SetextHeading, MarkdownBlockKind::TableCell})
    {
        const auto node = nodeAt(kind, first);
        if (node != MarkdownNoNode && (leaf == MarkdownNoNode || nodes[node].block.source.last - nodes[node].block.source.first <
                                                                     nodes[leaf].block.source.last - nodes[leaf].block.source.first))
            leaf = node;
    }
    if (leaf == MarkdownNoNode)
    {
        if (first != last || (first != 0u && text[first - 1u] != L'\r' && text[first - 1u] != L'\n') ||
            (first != text.size() && text[first] != L'\r' && text[first] != L'\n'))
            return std::nullopt;
        for (const auto kind : {MarkdownBlockKind::FencedCode, MarkdownBlockKind::IndentedCode, MarkdownBlockKind::HtmlBlock, MarkdownBlockKind::FrontMatter})
            if (nodeAt(kind, first) != MarkdownNoNode)
                return std::nullopt;
    }
    else if (last > nodes[leaf].block.content.last)
        return std::nullopt;
    for (const auto& node : syntax.Inlines())
    {
        if (node.kind == MarkdownInlineKind::Text || node.source.last <= first || node.source.first >= last)
            continue;
        const bool style =
            node.kind == MarkdownInlineKind::Strong || node.kind == MarkdownInlineKind::Emphasis || node.kind == MarkdownInlineKind::Strikethrough;
        const size_t count = node.kind == MarkdownInlineKind::Emphasis ? 1u : 2u;
        if (! style || node.source.last - node.source.first < count * 2u || first < node.source.first + count || last > node.source.last - count)
            return std::nullopt;
    }
    if (first == last)
        for (const auto& node : syntax.Inlines())
        {
            if (node.kind == MarkdownInlineKind::Text || node.source.first >= first || node.source.last <= first)
                continue;
            const bool style =
                node.kind == MarkdownInlineKind::Strong || node.kind == MarkdownInlineKind::Emphasis || node.kind == MarkdownInlineKind::Strikethrough;
            const size_t delimiter = node.kind == MarkdownInlineKind::Emphasis ? 1u : 2u;
            if (! style || node.source.last - node.source.first < delimiter * 2u || first < node.source.first + delimiter ||
                first > node.source.last - delimiter)
                return std::nullopt;
        }
    return leaf;
}
bool CanInsertMath(const MarkdownSyntax& syntax, Selection selection, std::wstring_view requested, bool display) noexcept
{
    if (syntax.Profile() != MarkdownProfile::RichMarkdown)
        return false;
    const auto site = FindInlineInsertionSite(syntax, selection);
    if (! site)
        return false;
    const auto leaf   = site.value();
    const auto text   = syntax.Source();
    const auto& nodes = syntax.Nodes();
    const auto first = selection.Start(), last = selection.End();
    const auto payload = first == last ? requested : text.substr(first, last - first);
    const auto closer  = display ? std::wstring_view(L"\\]") : std::wstring_view(L"\\)");
    if (payload.empty() || payload.size() > MathMaximumSourceUnits || ! IsValidUtf16(payload) || payload.find(closer) != payload.npos ||
        payload.size() + (display ? 8u : 4u) > Document::MaximumUnits - (text.size() - (last - first)))
        return false;
    if (display)
    {
        // The owner must be a complete physical paragraph line; preserve its
        // existing container prefix and generate canonical continuation lines.
        if (leaf != MarkdownNoNode && nodes[leaf].block.kind != MarkdownBlockKind::Paragraph)
            return false;
        const auto lineStart     = first ? text.find_last_of(L"\r\n", first - 1u) : text.npos;
        const auto physicalStart = lineStart == text.npos ? 0u : lineStart + 1u;
        const auto lineEnd       = text.find_first_of(L"\r\n", first);
        const auto physicalEnd   = lineEnd == text.npos ? text.size() : lineEnd;
        if (last != physicalEnd || (leaf == MarkdownNoNode ? first != physicalStart : std::ranges::none_of(nodes[leaf].lines, [&](const auto& line) {
            return line.source.first == first;
        })))
            return false;
        if (leaf != MarkdownNoNode)
            for (auto parent = nodes[leaf].parent; parent != MarkdownNoNode; parent = nodes[parent].parent)
                if (nodes[parent].block.kind != MarkdownBlockKind::Document && nodes[parent].block.kind != MarkdownBlockKind::BlockQuote &&
                    nodes[parent].block.kind != MarkdownBlockKind::List && nodes[parent].block.kind != MarkdownBlockKind::ListItem)
                    return false;
        // Multiline selected/requested values need line-by-line container
        // prefixing and cannot be committed as one selection-preserving edit.
        if (payload.find_first_of(L"\r\n") != payload.npos && first != physicalStart)
            return false;
        const auto prefixBound = (first - physicalStart) * 4u;
        if (prefixBound > (Document::MaximumUnits - (text.size() - (last - first)) - payload.size() - 8u) / 2u)
            return false;
        return true;
    }
    if (payload.find_first_of(L"\r\n") != payload.npos)
    {
        if (! display ||
            (leaf != MarkdownNoNode && (nodes[leaf].parent == MarkdownNoNode || nodes[nodes[leaf].parent].block.kind != MarkdownBlockKind::Document)))
            return false;
        size_t line = 0u;
        while (line <= payload.size())
        {
            const auto end      = payload.find_first_of(L"\r\n", line);
            const auto lastUnit = end == payload.npos ? payload.size() : end;
            if (payload.substr(line, lastUnit - line).find_first_not_of(L" \t") == payload.npos)
                return false; // A blank physical line would split the Markdown leaf.
            if (end == payload.npos)
                break;
            line = end + 1u;
            if (payload[end] == L'\r' && line < payload.size() && payload[line] == L'\n')
                ++line;
        }
    }
    return true;
}
bool CanInsertEscapedText(const MarkdownSyntax& syntax, Selection selection, std::wstring_view requested) noexcept
{
    if (! FindInlineInsertionSite(syntax, selection))
        return false;
    const auto text  = syntax.Source();
    const auto first = selection.Start(), last = selection.End();
    const auto payload = first == last ? requested : text.substr(first, last - first);
    if (payload.empty() || ! IsValidUtf16(payload) || payload.find_first_of(L"\r\n") != payload.npos || payload.size() > Document::MaximumUnits)
        return false;
    auto units = payload.size();
    for (const auto c : payload)
        if ((c >= L'!' && c <= L'/') || (c >= L':' && c <= L'@') || (c >= L'[' && c <= L'`') || (c >= L'{' && c <= L'~'))
        {
            if (units == Document::MaximumUnits)
                return false;
            ++units;
        }
    return units <= Document::MaximumUnits - (text.size() - (last - first));
}
std::optional<std::size_t> FindCodeLanguageTarget(const MarkdownSyntax& syntax, Selection selection, std::wstring_view language) noexcept
{
    if (language.size() > 255u || ! IsValidUtf16(language) || language.find_first_of(L"\r\n") != language.npos)
        return std::nullopt;
    const auto text   = syntax.Source();
    const auto& nodes = syntax.Nodes();
    for (std::size_t i = 1u; i < nodes.size(); ++i)
    {
        const auto& node = nodes[i];
        const auto range = node.block.source;
        if (node.block.kind != MarkdownBlockKind::FencedCode || node.parent == MarkdownNoNode || selection.Start() < range.first ||
            selection.End() > range.last ||
            ! (selection.Start() < range.last || (selection.Start() == range.last && range.last == text.size() && ! node.block.closed)))
            continue;
        if (node.block.fenceMarker == L'`' && language.find(L'`') != language.npos)
            return std::nullopt;
        const auto info = node.block.info;
        auto lineEnd    = text.find_first_of(L"\r\n", range.first);
        if (lineEnd == text.npos)
            lineEnd = text.size();
        if (info.first < range.first || info.first > info.last || info.last > lineEnd ||
            language.size() > Document::MaximumUnits - (text.size() - (info.last - info.first)))
            return std::nullopt;
        return i;
    }
    return std::nullopt;
}
static std::optional<std::wstring_view> CodeExitPrefix(const MarkdownSyntax& syntax, std::size_t index) noexcept
{
    const auto& nodes = syntax.Nodes();
    const auto& node  = nodes[index];
    bool inList       = false;
    for (auto parent = node.parent; parent != MarkdownNoNode; parent = nodes[parent].parent)
    {
        const auto kind = nodes[parent].block.kind;
        if (kind == MarkdownBlockKind::ListItem)
            inList = true;
        if (kind != MarkdownBlockKind::Document && kind != MarkdownBlockKind::BlockQuote && kind != MarkdownBlockKind::List &&
            kind != MarkdownBlockKind::ListItem)
            return std::nullopt;
    }
    if (nodes[node.parent].block.kind == MarkdownBlockKind::Document)
        return std::wstring_view{};
    const auto text = syntax.Source();
    if (inList)
    {
        std::size_t content{};
        if (node.block.closed)
        {
            content = node.block.source.last;
            if (content && text[content - 1u] == L'\n')
                --content;
            if (content && text[content - 1u] == L'\r')
                --content;
            while (content > node.block.source.first && (text[content - 1u] == L' ' || text[content - 1u] == L'\t'))
                --content;
            const auto fenceEnd = content;
            while (content > node.block.source.first && text[content - 1u] == node.block.fenceMarker)
                --content;
            if (fenceEnd - content < node.block.fenceLength)
                return std::nullopt;
        }
        else
        {
            if (node.lines.empty() || node.lines.back().leadingSpaces != 0u)
                return std::nullopt;
            content = node.lines.back().source.first;
            if (content >= text.size() || text[content] == L'\r' || text[content] == L'\n')
                return std::nullopt; // A blank line alone does not prove list indentation.
        }
        auto line = content;
        while (line && text[line - 1u] != L'\r' && text[line - 1u] != L'\n')
            --line;
        if (line == content)
            return std::nullopt;
        return text.substr(line, content - line);
    }
    auto marker = node.block.info.first;
    while (marker > node.block.source.first && (text[marker - 1u] == L' ' || text[marker - 1u] == L'\t'))
        --marker;
    const auto fenceEnd = marker;
    while (marker > node.block.source.first && text[marker - 1u] == node.block.fenceMarker)
        --marker;
    if (fenceEnd - marker < node.block.fenceLength)
        return std::nullopt;
    auto line = marker;
    while (line && text[line - 1u] != L'\r' && text[line - 1u] != L'\n')
        --line;
    return text.substr(line, marker - line);
}
std::optional<std::size_t> FindExitCodeBlock(const MarkdownSyntax& syntax, Selection selection) noexcept
{
    if (selection.Start() != selection.End())
        return std::nullopt;
    const auto& nodes = syntax.Nodes();
    const auto text   = syntax.Source();
    for (std::size_t i = 1u; i < nodes.size(); ++i)
    {
        const auto& node = nodes[i];
        const auto range = node.block.source;
        const auto caret = selection.caret;
        if (node.block.kind != MarkdownBlockKind::FencedCode || node.parent == MarkdownNoNode || range.first > caret ||
            ! (caret < range.last || (caret == range.last && caret == text.size() && ! node.block.closed)))
            continue;
        const auto prefix = CodeExitPrefix(syntax, i);
        if (! prefix)
            return std::nullopt;
        auto eol = text.find_first_of(L"\r\n", range.first);
        if (eol == text.npos)
            eol = text.find_first_of(L"\r\n");
        const std::size_t eolUnits = eol == text.npos ? 2u : text[eol] == L'\r' && eol + 1u < text.size() && text[eol + 1u] == L'\n' ? 2u : 1u;
        const bool hasEnd          = range.last > 0u && (text[range.last - 1u] == L'\r' || text[range.last - 1u] == L'\n');
        const auto inserted =
            (hasEnd ? 0u : eolUnits) + (node.block.closed ? 0u : prefix->size() + node.block.fenceLength + eolUnits) + prefix->size() + eolUnits;
        if (inserted > Document::MaximumUnits - text.size())
            return std::nullopt;
        return i;
    }
    return std::nullopt;
}
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
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4626 5027)
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
        if (syntax && syntax->SourceRegion())
            throw std::invalid_argument("Markdown commands require canonical syntax outside source editing");
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
        if (replacement.size() > Document::MaximumUnits || ! IsValidSourceUtf16(replacement))
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
                                 (text.empty() || (text.back() != L'\r' && text.back() != L'\n') ||
                                  ((kind == Kind::FencedCode || kind == Kind::FrontMatter) && ! nodes[i].block.closed)))) &&
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
        auto firstContent = line.first;
        while (firstContent < line.bodyEnd && Space(text[firstContent]))
            ++firstContent;
        if (firstContent == line.bodyEnd)
        {
            at = line.first;
            return true;
        }
        std::vector<std::size_t> parents(0u);
        for (auto i = nodes[node].parent; i != MarkdownNoNode; i = nodes[i].parent)
            if (nodes[i].block.kind == Kind::BlockQuote || nodes[i].block.kind == Kind::ListItem || nodes[i].block.kind == Kind::FootnoteDefinition)
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
            if (owner.block.kind == Kind::FootnoteDefinition)
            {
                if (Line(owner.block.source.first).first == line.first)
                {
                    if (owner.block.content.first < at || owner.block.content.first > line.bodyEnd)
                        return false;
                    column = Columns(Slice(line.first, owner.block.content.first));
                    at     = owner.block.content.first;
                }
                else if (! takeSpace(owner.contentIndent))
                    return false;
                continue;
            }
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
        const auto directFirst = nodes[item].firstChild;
        auto first             = directFirst;
        while (first != MarkdownNoNode && ! TextLeaf(nodes[first].block.kind) && nodes[first].firstChild != MarkdownNoNode)
            first = nodes[first].firstChild;
        if (directFirst != MarkdownNoNode && nodes[directFirst].block.kind == Kind::Paragraph && ! nodes[first].lines.empty() &&
            Line(nodes[first].lines.front().source.first).first == line.first)
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
    bool ContinuationPrefix(std::size_t leaf, const PhysicalLine& line, std::size_t at, std::wstring& prefix) const
    {
        std::vector<ItemPrefix> markers(0u);
        for (auto owner = Ancestor(leaf, Kind::ListItem); owner != MarkdownNoNode; owner = ParentItem(owner))
        {
            if (Line(nodes[owner].block.source.first).first != line.first)
                continue;
            ItemPrefix marker;
            if (! Prefix(owner, marker) || marker.first < line.first || marker.content > at)
                return false;
            markers.push_back(marker);
        }
        prefix.clear();
        auto cursor = line.first;
        for (auto marker = markers.rbegin(); marker != markers.rend(); ++marker)
        {
            if (marker->first < cursor)
                return false;
            prefix += Slice(cursor, marker->first);
            const auto before = Columns(Slice(line.first, marker->first));
            prefix.append(Columns(Slice(marker->first, marker->content), before) - before, L' ');
            cursor = marker->content;
        }
        prefix += Slice(cursor, at);
        return true;
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
    bool ShiftItem(std::size_t item, int delta, bool removeMarker, std::size_t indentationOwner = MarkdownNoNode, bool skipOpeningIndent = false)
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
            const auto& line  = lines[index];
            auto firstContent = line.first;
            while (firstContent < line.bodyEnd && Space(text[firstContent]))
                ++firstContent;
            if (firstContent == line.bodyEnd)
                continue;
            if (skipOpeningIndent && line.first == firstLine)
                continue;
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
            if (remove)
                delta = -static_cast<int>(nodes[items.front()].contentIndent);
            else
            {
                ItemPrefix childPrefix, parentPrefix;
                if (! Prefix(items.front(), childPrefix) || ! Prefix(parent, parentPrefix))
                    return;
                const auto outer       = parent;
                const auto& childLine  = Line(nodes[items.front()].block.source.first);
                const auto& parentLine = Line(nodes[parent].block.source.first);
                std::size_t childOuter{}, parentOuter{};
                if (! OuterOffset(outer, childLine, childOuter) || ! OuterOffset(outer, parentLine, parentOuter))
                    return;
                const auto childBase    = Columns(Slice(childLine.first, childOuter));
                const auto parentBase   = Columns(Slice(parentLine.first, parentOuter));
                const auto childIndent  = Columns(Slice(childOuter, childPrefix.first), childBase) - childBase;
                const auto parentIndent = Columns(Slice(parentOuter, parentPrefix.first), parentBase) - parentBase;
                if (childIndent <= parentIndent)
                    return;
                if (childLine.first == parentLine.first)
                {
                    // A nested list can begin in the parent's marker line
                    // (for example, "- - child"). Its outdent removes the
                    // parent's marker and padding, rather than trying to
                    // remove that width from nonexistent leading indentation.
                    const auto& owner = parent;
                    for (std::size_t index = 0u; index < items.size(); ++index)
                    {
                        ItemPrefix current;
                        if (! Prefix(items[index], current))
                        {
                            plan.edits_.clear();
                            return;
                        }
                        if (index == 0u)
                            Add(parentPrefix.first, current.first, L"");
                        if (! ShiftItem(items[index], -static_cast<int>(nodes[parent].contentIndent), false, owner, index == 0u))
                        {
                            plan.edits_.clear();
                            return;
                        }
                    }
                    plan.applicable_ = true;
                    return;
                }
                delta            = static_cast<int>(parentIndent) - static_cast<int>(childIndent);
                const auto owner = outer;
                for (const auto item : items)
                    if (! ShiftItem(item, delta, false, owner))
                    {
                        plan.edits_.clear();
                        return;
                    }
                plan.applicable_ = true;
                return;
            }
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
    bool NoteContinuation(std::wstring& prefix, const PhysicalLine& line, std::size_t source) const
    {
        const auto note = NodeAt(Kind::FootnoteDefinition, source);
        if (note == MarkdownNoNode || Line(nodes[note].block.source.first).first != line.first)
            return true;
        std::size_t outer{};
        if (! OuterOffset(note, line, outer) || nodes[note].block.content.first < outer || nodes[note].block.content.first - line.first > prefix.size())
            return false;
        prefix.replace(outer - line.first, nodes[note].block.content.first - outer, nodes[note].contentIndent, L' ');
        return true;
    }
    void Continue()
    {
        const auto first = plan.before_.Start(), last = plan.before_.End();
        const auto& line = Line(first);
        if (! IsSingleLineCommandSelection(*plan.syntax_, plan.before_))
            return;
        for (const auto kind : {Kind::FencedCode, Kind::IndentedCode, Kind::HtmlBlock, Kind::FrontMatter})
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
            std::wstring prefix(Slice(line.first, at));
            if (! NoteContinuation(prefix, line, first))
                return;
            std::wstring inserted(Eol(line));
            inserted += prefix;
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
            const auto nestedQuote = NodeAt(Kind::BlockQuote, first);
            if (nestedQuote != MarkdownNoNode && Ancestor(nestedQuote, Kind::ListItem) == item)
                return; // Retain ambiguous nested quote/list prefixes until their continuation policy is explicit.
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
                    if (at == line.bodyEnd && first == at)
                    {
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
        }
        if (! NoteContinuation(prefix, line, first))
            return;
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
    void Quote(MarkdownQuoteAction action);
    void CodeBlock(const MarkdownCommandRequest& request);
    void ExitCode();
    void HardBreak();
    void Math(const MarkdownCommandRequest& request);
    void ThematicBreak();
    void Image(const MarkdownCommandRequest& request);
    void ClearFormatting();
    void EscapedText(const MarkdownCommandRequest& request);
    void Join(bool backwards);
    void Style(MarkdownTextStyle style);
    void Link(const MarkdownCommandRequest& request, bool remove);
    void Task();
    void Table(const MarkdownCommandRequest& request);
    void Delete(std::span<const MarkdownRange> ranges);
    std::optional<MarkdownRange> InlineBody(const MarkdownInline& node) const;
    bool InlineSelection(std::size_t& leaf) const
    {
        leaf = LeafAt(plan.before_.Start());
        if (leaf == MarkdownNoNode && plan.before_.anchor == plan.before_.caret && Line(plan.before_.caret).first == Line(plan.before_.caret).bodyEnd)
            return NodeAt(Kind::FencedCode, plan.before_.caret) == MarkdownNoNode && NodeAt(Kind::IndentedCode, plan.before_.caret) == MarkdownNoNode &&
                   NodeAt(Kind::HtmlBlock, plan.before_.caret) == MarkdownNoNode && NodeAt(Kind::FrontMatter, plan.before_.caret) == MarkdownNoNode;
        return leaf != MarkdownNoNode && plan.before_.End() <= nodes[leaf].block.content.last;
    }
    bool EmptyStructuralSite() const
    {
        if (plan.before_.anchor != plan.before_.caret)
            return false;
        for (const auto kind : {Kind::FencedCode, Kind::IndentedCode, Kind::HtmlBlock, Kind::FrontMatter, Kind::Table})
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
            else if (node.kind == MarkdownInlineKind::Link || node.kind == MarkdownInlineKind::Math || node.kind == MarkdownInlineKind::FootnoteReference)
                return std::nullopt;
        }
        return std::optional<std::wstring>(std::in_place, output.text);
    }
    bool HasOpaqueSelection() const
    {
        for (const auto& node : nodes)
            if (node.block.source.first < plan.before_.End() && node.block.source.last > plan.before_.Start() &&
                (node.block.kind == Kind::FencedCode || node.block.kind == Kind::IndentedCode || node.block.kind == Kind::HtmlBlock ||
                 node.block.kind == Kind::FrontMatter || node.block.kind == Kind::Table || node.block.kind == Kind::ThematicBreak))
                return true;
        return false;
    }
    bool NeedsUnwrapEscape(std::size_t leaf, MarkdownRange selected, MarkdownRange unit, wchar_t value) const
    {
        if (value == L'|' && nodes[leaf].block.kind == Kind::TableCell)
            return true;
        if ((value == L'*' || value == L'_' || value == L'~') && (unit.first == selected.first || unit.last == selected.last))
            return true;

        const auto& line             = Line(unit.first);
        const auto lineSelectedFirst = std::max(selected.first, line.first);
        const auto lineSelectedLast  = std::min(selected.last, line.bodyEnd);
        auto first                   = lineSelectedFirst;
        while (first < lineSelectedLast && Space(text[first]))
            ++first;
        if (unit.first != first)
            return false;
        const auto after = [&](std::size_t at) { return at >= line.bodyEnd || Space(text[at]); };
        if ((value == L'#' || value == L'>' || value == L'-' || value == L'+' || value == L'*') && after(unit.last))
            return true;
        if (Digit(value))
        {
            auto end = unit.first;
            while (end < lineSelectedLast && Digit(text[end]))
                ++end;
            if (end - unit.first <= 9u && end < line.bodyEnd && (text[end] == L'.' || text[end] == L')') && after(end + 1u))
                return true;
        }
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
                if (stageEscapes && ! NeedsUnwrapEscape(leaf, selected, unit.source, value.text[i]))
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
    void RestrictToNote(std::size_t note)
    {
        if (note == MarkdownNoNode || ! plan.applicable_)
            return;
        const auto content = nodes[note].block.content.first;
        const auto end     = nodes[note].block.source.last;
        for (const auto& edit : plan.edits_)
        {
            if (edit.source.first < content || edit.source.last > end)
            {
                plan.applicable_ = false;
                return;
            }
            const auto& boundaryLine = Line(edit.source.first);
            const auto& line         = edit.source.first == edit.source.last && edit.source.first == boundaryLine.first && edit.source.first > content
                                           ? Line(edit.source.first - 1u)
                                           : boundaryLine;
            std::size_t outer{};
            if (! OuterOffset(note, line, outer))
            {
                plan.applicable_ = false;
                return;
            }
            if (Line(nodes[note].block.source.first).first != line.first)
            {
                auto at           = outer;
                unsigned column   = Columns(Slice(line.first, outer));
                const auto target = column + nodes[note].contentIndent;
                while (at < line.bodyEnd && Space(text[at]) && column < target)
                {
                    const auto next = text[at] == L'\t' ? column + 4u - column % 4u : column + 1u;
                    if (next > target)
                        break;
                    column = next;
                    ++at;
                }
                if (column != target ||
                    (edit.source.first < at && ! (edit.source.first == line.first && edit.source.last >= line.end && edit.replacement.empty())))
                {
                    plan.applicable_ = false;
                    return;
                }
            }
            std::wstring continuation(Slice(line.first, outer));
            continuation.append(nodes[note].contentIndent, L' ');
            for (std::size_t at = 0u; at < edit.replacement.size(); ++at)
            {
                if (edit.replacement[at] != L'\r' && edit.replacement[at] != L'\n')
                    continue;
                if (edit.replacement[at] == L'\r' && at + 1u < edit.replacement.size() && edit.replacement[at + 1u] == L'\n')
                    ++at;
                const auto remainder = std::wstring_view(edit.replacement).substr(at + 1u);
                if (! remainder.empty() ? ! remainder.starts_with(continuation)
                                        : (edit.source.last < end && ! text.substr(edit.source.last).starts_with(continuation)))
                {
                    plan.applicable_ = false;
                    return;
                }
            }
        }
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
        if (! IsValidSourceUtf16(candidate) || ! IsEditBoundary(candidate, plan.after_.anchor) || ! IsEditBoundary(candidate, plan.after_.caret))
            throw std::out_of_range("Invalid structural command result selection");
        return plan;
    }
};
#pragma warning(pop)

void MarkdownCommandPlanner::Quote(MarkdownQuoteAction action)
{
    if (action != MarkdownQuoteAction::Toggle && action != MarkdownQuoteAction::Indent && action != MarkdownQuoteAction::Outdent)
        throw std::invalid_argument("Invalid quote action");
    struct SelectedLine
    {
        std::size_t first, at;
        bool quoted;
    };
    std::vector<SelectedLine> selected(0u);
    auto end = plan.before_.End();
    if (end > plan.before_.Start() && Line(end).first == end)
        --end;
    for (std::size_t i = 1u; i < nodes.size(); ++i)
    {
        if (nodes[i].block.kind == Kind::FencedCode || nodes[i].block.kind == Kind::IndentedCode || nodes[i].block.kind == Kind::HtmlBlock ||
            nodes[i].block.kind == Kind::Table || nodes[i].block.kind == Kind::ThematicBreak || nodes[i].block.kind == Kind::ReferenceDefinition)
        {
            const auto range      = nodes[i].block.source;
            const bool intersects = plan.before_.Start() == plan.before_.End()
                                        ? range.first <= end && (end < range.last || (end == range.last && end == text.size() && ! nodes[i].block.closed))
                                        : range.first < plan.before_.End() && range.last > plan.before_.Start();
            if (! intersects)
                continue;
            for (auto line = std::ranges::lower_bound(lines, range.first, {}, &PhysicalLine::first); line != lines.end() && line->first < range.last; ++line)
            {
                std::size_t at{};
                if (! OuterOffset(i, *line, at))
                    return;
                selected.push_back({line->first, at, Ancestor(i, Kind::BlockQuote) != MarkdownNoNode});
            }
            continue;
        }
        if (! TextLeaf(nodes[i].block.kind) || nodes[i].block.kind == Kind::TableCell)
            continue;
        for (const auto& content : nodes[i].lines)
        {
            const auto& line = Line(content.source.first);
            if ((plan.before_.Start() == plan.before_.End() ? ! (line.first <= end && end <= line.bodyEnd)
                                                            : line.end <= plan.before_.Start() || line.first > end))
                continue;
            std::size_t at{};
            if (! OuterOffset(i, line, at))
                return;
            selected.push_back({line.first, at, Ancestor(i, Kind::BlockQuote) != MarkdownNoNode});
        }
    }
    for (const auto& empty : plan.syntax_->EmptyLines())
    {
        const auto& line = Line(empty.source.first);
        const bool included =
            plan.before_.Start() == plan.before_.End() ? line.first <= end && end <= line.bodyEnd : line.end > plan.before_.Start() && line.first <= end;
        if (! included)
            continue;
        if (empty.owner >= nodes.size() || empty.insertion < line.first || empty.insertion > line.bodyEnd)
            return;
        selected.push_back({line.first, empty.insertion, Ancestor(empty.owner, Kind::BlockQuote) != MarkdownNoNode});
    }
    if (selected.empty())
    {
        if (action != MarkdownQuoteAction::Outdent && EmptyStructuralSite())
        {
            Add(plan.before_.caret, plan.before_.caret, L"> ");
            plan.applicable_ = true;
        }
        return;
    }
    std::ranges::sort(selected, {}, &SelectedLine::first);
    for (std::size_t i = 1u; i < selected.size(); ++i)
        if (selected[i - 1u].first == selected[i].first)
            return;
    for (const auto& line : lines)
    {
        const bool included =
            plan.before_.Start() == plan.before_.End() ? line.first <= end && end <= line.bodyEnd : line.end > plan.before_.Start() && line.first <= end;
        if (included)
        {
            const auto found = std::ranges::lower_bound(selected, line.first, {}, &SelectedLine::first);
            if (found == selected.end() || found->first != line.first)
                return; // Never apply only part of a selection containing unsupported blocks or trivia.
        }
    }
    const bool allQuoted = std::ranges::all_of(selected, &SelectedLine::quoted);
    const bool anyQuoted = std::ranges::any_of(selected, &SelectedLine::quoted);
    if (action == MarkdownQuoteAction::Outdent && ! allQuoted)
        return;
    const bool remove = action == MarkdownQuoteAction::Outdent || (action == MarkdownQuoteAction::Toggle && allQuoted);
    for (const auto& line : selected)
    {
        if (action == MarkdownQuoteAction::Toggle && anyQuoted && ! allQuoted && line.quoted)
            continue;
        if (! remove)
            Add(line.at, line.at, L"> ");
        else
        {
            const auto marker = text.rfind(L'>', line.at ? line.at - 1u : 0u);
            if (marker == text.npos || marker < line.first || marker >= line.at)
            {
                plan.edits_.clear();
                return;
            }
            auto last = marker + 1u;
            if (last < line.at && (text[last] == L' ' || text[last] == L'\t'))
                ++last;
            Add(marker, last, L"");
        }
    }
    plan.applicable_ = true;
}

void MarkdownCommandPlanner::CodeBlock(const MarkdownCommandRequest& request)
{
    if (request.text.size() > 255u || ! IsValidUtf16(request.text) || request.text.find_first_of(L"\r\n") != request.text.npos)
        throw std::invalid_argument("Invalid code language");
    if (request.command == MarkdownCommand::SetCodeLanguage)
    {
        const auto target = FindCodeLanguageTarget(*plan.syntax_, plan.before_, request.text);
        if (! target)
            return;
        const auto info = nodes[target.value()].block.info;
        Add(info.first, info.last, request.text);
        plan.applicable_ = true;
        return;
    }
    const auto first = plan.before_.Start(), last = plan.before_.End();
    const auto fenced = NodeAt(Kind::FencedCode, first);
    if (fenced != MarkdownNoNode)
    {
        const auto& node = nodes[fenced];
        if (node.parent == MarkdownNoNode || last > node.block.source.last)
            return;
        const auto& opening = Line(node.block.source.first);
        if (node.block.source.last < opening.end)
            return;
        const auto& closing = Line(node.block.source.last ? node.block.source.last - 1u : 0u);
        if (node.block.closed && closing.first < opening.end)
            return;
        const auto bodyEnd         = node.block.closed ? closing.first : node.block.source.last;
        bool openingOwnsListMarker = false;
        for (auto parent = node.parent; parent != MarkdownNoNode; parent = nodes[parent].parent)
        {
            const auto kind = nodes[parent].block.kind;
            if (kind != Kind::Document && kind != Kind::BlockQuote && kind != Kind::List && kind != Kind::ListItem)
                return;
            if (kind == Kind::ListItem && Line(nodes[parent].block.source.first).first == opening.first)
                openingOwnsListMarker = true;
        }
        if (openingOwnsListMarker)
        {
            std::size_t markerEnd{};
            if (! OuterOffset(fenced, opening, markerEnd))
                return;
            auto marker = markerEnd;
            while (marker < opening.bodyEnd && Space(text[marker]))
                ++marker;
            if (marker == opening.bodyEnd || text[marker] != node.block.fenceMarker)
                return;
            if (opening.end == bodyEnd)
            {
                std::wstring emptyItem(Slice(opening.first, markerEnd));
                emptyItem += node.block.closed ? Slice(closing.bodyEnd, closing.end) : Slice(opening.bodyEnd, opening.end);
                Add(opening.first, node.block.closed ? closing.end : opening.end, emptyItem);
                plan.applicable_ = true;
                return;
            }
            if (opening.end > bodyEnd)
                return;
            std::size_t bodyStart{};
            const auto& firstBody = Line(opening.end);
            if (! OuterOffset(fenced, firstBody, bodyStart) || bodyStart > firstBody.bodyEnd)
                return;
            Add(opening.first, bodyStart, Slice(opening.first, markerEnd));
        }
        else if (! node.block.closed && opening.end == bodyEnd && Ancestor(fenced, Kind::BlockQuote) != MarkdownNoNode)
        {
            std::size_t prefixEnd{};
            if (! OuterOffset(fenced, opening, prefixEnd))
                return;
            std::wstring emptyQuote(Slice(opening.first, prefixEnd));
            emptyQuote += Slice(opening.bodyEnd, opening.end);
            Add(opening.first, opening.end, emptyQuote);
        }
        else
            Add(opening.first, opening.end, L"");
        if (node.block.closed)
            Add(closing.first, closing.end, L"");
        plan.applicable_ = true;
        return;
    }
    auto end = last;
    if (end > first && Line(end).first == end)
        --end;
    const auto& firstLine = Line(first);
    const auto& lastLine  = Line(end);
    const auto begin = firstLine.first, finish = lastLine.end;
    std::wstring containerPrefix(0u, L' ');
    std::wstring continuationPrefix(0u, L' ');
    std::size_t firstContent = begin;
    const auto leaf          = NodeAt(Kind::Paragraph, first);
    if (leaf != MarkdownNoNode && nodes[leaf].parent != MarkdownNoNode && nodes[nodes[leaf].parent].block.kind != Kind::Document)
    {
        if (last > nodes[leaf].block.source.last)
            return;
        for (auto parent = nodes[leaf].parent; parent != MarkdownNoNode; parent = nodes[parent].parent)
            if (nodes[parent].block.kind != Kind::Document && nodes[parent].block.kind != Kind::BlockQuote && nodes[parent].block.kind != Kind::List &&
                nodes[parent].block.kind != Kind::ListItem)
                return;
        if (! OuterOffset(leaf, firstLine, firstContent) || ! ContinuationPrefix(leaf, firstLine, firstContent, continuationPrefix))
            return;
        containerPrefix = Slice(firstLine.first, firstContent);
        for (const auto& line : lines)
        {
            if (line.first <= begin || line.first >= finish)
                continue;
            std::size_t content{};
            if (! OuterOffset(leaf, line, content) || Slice(line.first, content) != continuationPrefix)
                return;
        }
    }
    for (std::size_t i = 1u; i < nodes.size(); ++i)
    {
        const auto& node = nodes[i];
        if (node.block.source.last <= begin || node.block.source.first >= finish)
            continue;
        if (node.block.kind != Kind::Paragraph && node.block.kind != Kind::BlankLine &&
            (containerPrefix.empty() || (node.block.kind != Kind::BlockQuote && node.block.kind != Kind::List && node.block.kind != Kind::ListItem)))
            return;
    }
    size_t longest = 0u, run = 0u;
    for (const wchar_t ch : Slice(begin, finish))
    {
        run     = ch == L'~' ? run + 1u : 0u;
        longest = std::max(longest, run);
    }
    const std::wstring fence(std::max(size_t{3u}, longest + 1u), L'~');
    std::wstring opening = containerPrefix;
    opening += fence;
    opening += request.text;
    opening += Eol(firstLine);
    std::wstring closing(0u, L' ');
    if (lastLine.end == lastLine.bodyEnd)
        closing += Eol(lastLine);
    closing += continuationPrefix;
    closing += fence;
    if (lastLine.end != lastLine.bodyEnd)
        closing += Eol(lastLine);
    if (begin == finish)
    {
        opening += closing;
        Add(begin, begin, opening);
    }
    else
    {
        if (containerPrefix == continuationPrefix)
            Add(begin, begin, opening);
        else
        {
            opening += continuationPrefix;
            Add(begin, firstContent, opening);
        }
        Add(finish, finish, closing);
    }
    plan.applicable_ = true;
}

void MarkdownCommandPlanner::HardBreak()
{
    const auto first = plan.before_.Start(), last = plan.before_.End();
    const auto caret = first;
    const auto leaf  = LeafAt(caret);
    if (leaf == MarkdownNoNode || nodes[leaf].block.kind != Kind::Paragraph || last > nodes[leaf].block.content.last ||
        ! IsSingleLineCommandSelection(*plan.syntax_, plan.before_))
        return;
    for (const auto& node : plan.syntax_->Inlines())
    {
        const bool intersects = first == last ? node.source.first < first && first < node.source.last : node.source.first < last && node.source.last > first;
        if (node.kind == MarkdownInlineKind::Text || ! intersects)
            continue;
        const bool style =
            node.kind == MarkdownInlineKind::Strong || node.kind == MarkdownInlineKind::Emphasis || node.kind == MarkdownInlineKind::Strikethrough;
        const auto body = style ? InlineBody(node) : std::nullopt;
        if (! body || first < body->first || last > body->last)
            return;
    }
    const auto& line = Line(caret);
    std::size_t at{};
    if (caret > line.bodyEnd || ! OuterOffset(leaf, line, at) || caret < at)
        return;
    std::wstring prefix(0u, L' ');
    if (! ContinuationPrefix(leaf, line, at, prefix))
        return;
    std::wstring insertion(L"  ");
    insertion += Eol(line);
    insertion += prefix;
    Add(first, last, insertion);
    plan.after_       = {caret + insertion.size(), caret + insertion.size()};
    explicitSelection = true;
    plan.applicable_  = true;
}

void MarkdownCommandPlanner::ExitCode()
{
    const auto found = FindExitCodeBlock(*plan.syntax_, plan.before_);
    if (! found)
        return;
    const auto& node  = nodes[found.value()];
    const auto prefix = CodeExitPrefix(*plan.syntax_, found.value()).value();
    const auto at     = node.block.source.last;
    const auto eol    = Eol(Line(node.block.source.first));
    const bool hasEnd = at > 0u && (text[at - 1u] == L'\r' || text[at - 1u] == L'\n');
    std::wstring insertion(0u, L' ');
    if (! hasEnd)
        insertion += eol;
    if (! node.block.closed)
    {
        insertion += prefix;
        insertion.append(node.block.fenceLength, node.block.fenceMarker);
        insertion += eol;
    }
    insertion += prefix;
    const auto caret = at + insertion.size();
    insertion += eol;
    Add(at, at, insertion);
    plan.after_       = {caret, caret};
    explicitSelection = true;
    plan.applicable_  = true;
}

void MarkdownCommandPlanner::Math(const MarkdownCommandRequest& request)
{
    if (! CanInsertMath(*plan.syntax_, plan.before_, request.text, request.mathDisplay))
        return;
    const auto first = plan.before_.Start(), last = plan.before_.End();
    const auto payload = first == last ? request.text : Slice(first, last);
    const auto closer  = request.mathDisplay ? std::wstring_view(L"\\]") : std::wstring_view(L"\\)");
    const auto& line   = Line(first);
    const auto newline = Eol(line);
    std::wstring continuation(0u, L' ');
    if (request.mathDisplay)
    {
        const auto site = FindInlineInsertionSite(*plan.syntax_, plan.before_);
        if (! site || (site.value() != MarkdownNoNode && ! ContinuationPrefix(site.value(), line, first, continuation)))
            return;
    }
    std::wstring insertion = request.mathDisplay ? L"\\[" : L"\\(";
    if (request.mathDisplay)
    {
        insertion += newline;
        insertion += continuation;
    }
    insertion += payload;
    if (request.mathDisplay)
    {
        insertion += newline;
        insertion += continuation;
    }
    insertion += closer;
    Add(first, last, insertion);
    const auto begin = first + 2u + (request.mathDisplay ? newline.size() + continuation.size() : 0u), end = begin + payload.size();
    plan.after_       = plan.before_.anchor <= plan.before_.caret ? Selection{begin, end} : Selection{end, begin};
    explicitSelection = true;
    plan.applicable_  = true;
}

namespace
{
std::wstring LiteralMarkdown(std::wstring_view value);
}
void MarkdownCommandPlanner::ClearFormatting()
{
    if (plan.before_.Start() == plan.before_.End())
        return;
    size_t leaf{};
    if (! InlineSelection(leaf))
        return;
    const auto first = plan.before_.Start(), last = plan.before_.End();
    const MarkdownInline* only = nullptr;
    for (const auto& node : plan.syntax_->Inlines())
    {
        if (node.kind == MarkdownInlineKind::Text || node.source.first >= last || node.source.last <= first)
            continue;
        if (only)
        {
            only = nullptr;
            break;
        }
        only = &node;
    }
    if (only && only->kind == MarkdownInlineKind::Link && text[only->source.first] != L'[')
    {
        const auto body = InlineBody(*only);
        if (! body || first > body->first || last < body->last)
            return;
        // Escape the visible autolink spelling so removing its delimiters does
        // not immediately create another angle or extended autolink.
        const auto replacement = LiteralMarkdown(Slice(body->first, body->last));
        Add(only->source.first, only->source.last, replacement);
        const auto end    = only->source.first + replacement.size();
        plan.after_       = plan.before_.anchor <= plan.before_.caret ? Selection{only->source.first, end} : Selection{end, only->source.first};
        explicitSelection = true;
        plan.applicable_  = true;
        return;
    }
    if (only && (only->kind == MarkdownInlineKind::Strong || only->kind == MarkdownInlineKind::Emphasis || only->kind == MarkdownInlineKind::Strikethrough ||
                 only->kind == MarkdownInlineKind::Code))
    {
        const auto body = InlineBody(*only);
        if (body && (only->kind == MarkdownInlineKind::Code || first > body->first || last < body->last))
        {
            const auto style = only->kind == MarkdownInlineKind::Strong          ? MarkdownTextStyle::Strong
                               : only->kind == MarkdownInlineKind::Emphasis      ? MarkdownTextStyle::Emphasis
                               : only->kind == MarkdownInlineKind::Strikethrough ? MarkdownTextStyle::Strikethrough
                                                                                 : MarkdownTextStyle::Code;
            Style(style);
            return;
        }
    }
    std::vector<MarkdownRange> removals(0u);
    for (const auto& node : plan.syntax_->Inlines())
    {
        if (node.source.first >= last || node.source.last <= first || node.kind == MarkdownInlineKind::Text)
            continue;
        if (node.kind != MarkdownInlineKind::Strong && node.kind != MarkdownInlineKind::Emphasis && node.kind != MarkdownInlineKind::Strikethrough &&
            node.kind != MarkdownInlineKind::Link)
            return;
        if (node.kind == MarkdownInlineKind::Link && text[node.source.first] != L'[')
            return; // Bare/autolinks would become links again without escaping.
        const auto body = InlineBody(node);
        if (! body || first > body->first || last < body->last)
            return;
        removals.push_back({node.source.first, body->first});
        removals.push_back({body->last, node.source.last});
    }
    std::ranges::sort(removals, {}, &MarkdownRange::first);
    for (size_t i = 1u; i < removals.size(); ++i)
        if (removals[i].first < removals[i - 1u].last || removals[i].first == removals[i - 1u].first)
            return;
    for (const auto range : removals)
        Add(range.first, range.last, L"");
    plan.applicable_ = true;
}

void MarkdownCommandPlanner::ThematicBreak()
{
    const auto admission = InspectThematicBreak(*plan.syntax_, plan.before_);
    if (! admission)
        return;
    const auto caret = plan.before_.caret;
    std::wstring insertion(0u, L' ');
    for (unsigned i = 0u; i < admission->before; ++i)
        insertion += admission->eol;
    insertion += L"---";
    for (unsigned i = 0u; i < admission->after; ++i)
        insertion += admission->eol;
    Add(caret, caret, insertion);
    plan.after_       = {caret + insertion.size(), caret + insertion.size()};
    explicitSelection = true;
    plan.applicable_  = true;
}

void MarkdownCommandPlanner::SetList(const MarkdownCommandRequest& request)
{
    if (HasOpaqueSelection())
        return;
    if (request.listKind != MarkdownListKind::None && request.listKind != MarkdownListKind::Unordered && request.listKind != MarkdownListKind::Ordered &&
        request.listKind != MarkdownListKind::Task)
        throw std::invalid_argument("Invalid Markdown list kind");
    if (request.listStart && *request.listStart > 999999999u)
        throw std::out_of_range("Ordered list start exceeds nine digits");
    if (! IsCommandFeatureEnabled(*plan.syntax_, MarkdownCommand::SetList, MarkdownTextStyle::Strong, request.listKind))
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
            if (request.listKind == MarkdownListKind::Task &&
                (nodes[item].firstChild == MarkdownNoNode || nodes[nodes[item].firstChild].block.kind != Kind::Paragraph))
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
            if (request.listKind == MarkdownListKind::Ordered && number > 999999999u)
            {
                plan.edits_.clear();
                return;
            }
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
    if (! IsCommandFeatureEnabled(*plan.syntax_, MarkdownCommand::ToggleTask, MarkdownTextStyle::Strong, MarkdownListKind::None))
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
    const auto found = FindJoinLeaf(*plan.syntax_, plan.before_, backwards);
    if (! found)
        return;
    const auto current = found.value();
    if (backwards && (nodes[current].block.kind == Kind::AtxHeading || nodes[current].block.kind == Kind::SetextHeading))
    {
        Heading(0u);
        return;
    }
    if (nodes[current].block.kind != Kind::Paragraph)
        return;
    if (backwards)
    {
        const auto quote = Ancestor(current, Kind::BlockQuote);
        auto nearest     = nodes[current].parent;
        while (nearest != MarkdownNoNode && nodes[nearest].block.kind != Kind::BlockQuote && nodes[nearest].block.kind != Kind::ListItem)
            nearest = nodes[nearest].parent;
        if (quote != MarkdownNoNode && nearest == quote && nodes[quote].firstChild == current)
        {
            const auto markerFor = [&](const PhysicalLine& line) -> std::optional<MarkdownRange>
            {
                std::size_t at{};
                if (! OuterOffset(quote, line, at))
                    return std::nullopt;
                while (at < line.bodyEnd && Space(text[at]))
                    ++at;
                if (at == line.bodyEnd || text[at] != L'>')
                    return MarkdownRange{at, at}; // Parser-owned lazy continuation has no marker to remove.
                auto last = at + 1u;
                if (last < line.bodyEnd && Space(text[last]))
                    ++last;
                return MarkdownRange{at, last};
            };
            const auto first = std::ranges::lower_bound(lines, Line(nodes[quote].block.source.first).first, {}, &PhysicalLine::first);
            const auto last  = std::ranges::lower_bound(lines, nodes[quote].block.source.last, {}, &PhysicalLine::first);
            if (first == last || std::ranges::any_of(first, last, [&](const auto& line) { return ! markerFor(line); }))
                return; // An unproven outer prefix cannot be outdented safely.
            const auto opening = markerFor(*first).value();
            if (opening.first == opening.last)
                return;
            for (auto line = first; line != last; ++line)
            {
                const auto marker = markerFor(*line).value();
                if (marker.first != marker.last)
                    Add(marker.first, marker.last, L"");
            }
            plan.applicable_ = true;
            return;
        }
    }
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
void MarkdownCommandPlanner::EscapedText(const MarkdownCommandRequest& request)
{
    if (! CanInsertEscapedText(*plan.syntax_, plan.before_, request.text))
        return;
    const auto first = plan.before_.Start(), last = plan.before_.End();
    const auto payload   = first == last ? request.text : Slice(first, last);
    const auto insertion = LiteralMarkdown(payload);
    Add(first, last, insertion);
    const auto end    = first + insertion.size();
    plan.after_       = {end, end};
    explicitSelection = true;
    plan.applicable_  = true;
}
void MarkdownCommandPlanner::Image(const MarkdownCommandRequest& request)
{
    const bool relative = NativeHtmlDestination(request.destination) && request.destination.find(L':') == request.destination.npos;
    if (request.destination.empty() || (! relative && ! ResolveRemoteImageSource(request.destination) && ! ResolveLocalImagePath({}, request.destination)))
        throw std::invalid_argument("Ineligible image destination");
    std::size_t leaf{};
    if (! InlineSelection(leaf))
        return;
    const auto first = plan.before_.Start(), last = plan.before_.End();
    for (const auto& node : plan.syntax_->Inlines())
    {
        const bool intersects = first == last ? node.source.first < first && first < node.source.last : node.source.first < last && node.source.last > first;
        if (node.kind == MarkdownInlineKind::Text || ! intersects)
            continue;
        const bool style =
            node.kind == MarkdownInlineKind::Strong || node.kind == MarkdownInlineKind::Emphasis || node.kind == MarkdownInlineKind::Strikethrough;
        const auto body = style ? InlineBody(node) : std::nullopt;
        if (! body || first < body->first || last > body->last)
            return;
    }
    const auto alt = request.text.empty() ? Slice(first, last) : request.text;
    if (! IsValidUtf16(alt) || alt.find_first_of(L"\r\n") != alt.npos)
        return;
    std::wstring insertion(L"![");
    insertion += LiteralMarkdown(alt);
    insertion += L"](<";
    insertion += AttributeSource(request.destination);
    insertion += L'>';
    if (! request.title.empty())
    {
        insertion += L" \"";
        insertion += AttributeSource(request.title);
        insertion += L'\"';
    }
    insertion += L')';
    Add(first, last, insertion);
    const auto end    = first + insertion.size();
    plan.after_       = {end, end};
    explicitSelection = true;
    plan.applicable_  = true;
}
void MarkdownCommandPlanner::Style(MarkdownTextStyle style)
{
    if (style != MarkdownTextStyle::Strong && style != MarkdownTextStyle::Emphasis && style != MarkdownTextStyle::Strikethrough &&
        style != MarkdownTextStyle::Code)
        throw std::invalid_argument("Invalid Markdown text style");
    if (! IsCommandFeatureEnabled(*plan.syntax_, MarkdownCommand::ToggleStyle, style, MarkdownListKind::None))
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
    const auto first = plan.before_.Start(), last = plan.before_.End();
    for (const auto& node : plan.syntax_->Inlines())
        if (node.kind != MarkdownInlineKind::Text && node.source.first < plan.before_.End() && node.source.last > plan.before_.Start() &&
            (plan.before_.Start() > node.source.first || plan.before_.End() < node.source.last))
        {
            const bool eligibleAncestorStyle =
                node.kind == MarkdownInlineKind::Strong || node.kind == MarkdownInlineKind::Emphasis || node.kind == MarkdownInlineKind::Strikethrough;
            const auto body = eligibleAncestorStyle ? InlineBody(node) : std::nullopt;
            // Nested formatting may be applied to a complete source selection
            // inside an existing style body (including entity source spelling).
            // Crossing the style delimiters or any other non-text token remains
            // unavailable so the edit cannot claim partial ownership.
            if (! body || first < body->first || last > body->last)
                return;
        }
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
                    const auto whitespaceFirst = at;
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
                        const auto value = request.title.empty() ? range(whitespaceFirst, at) : range(first, last);
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
            else if (node.kind != MarkdownInlineKind::Text && node.source.first < owned.last && node.source.last > owned.first &&
                     (owned.first > node.source.first || owned.last < node.source.last))
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
    if (! IsCommandFeatureEnabled(*plan.syntax_, request.command, MarkdownTextStyle::Strong, MarkdownListKind::None))
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
                NodeAt(Kind::HtmlBlock, at) != MarkdownNoNode || NodeAt(Kind::FrontMatter, at) != MarkdownNoNode)
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
        if (! NoteContinuation(prefix, line, plan.before_.Start()))
            return;
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
    const auto removeTable = [&]
    {
        auto first      = nodes[table].block.source.first;
        const auto note = Ancestor(table, Kind::FootnoteDefinition);
        std::wstring replacement(0u, L' ');
        if (note != MarkdownNoNode && first < nodes[note].block.content.first)
        {
            first = nodes[note].block.content.first;
            replacement.assign(Eol(Line(first)));
        }
        Add(first, nodes[table].block.source.last, replacement);
        plan.after_       = {first, first};
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
            if (! IsValidSourceUtf16(request.text) || request.text.find_first_of(L"\r\n") != std::wstring_view::npos)
                throw std::invalid_argument("Cell source must be valid single-line Unicode");
            if (request.tableCellCaretOffset && ! IsEditBoundary(request.text, request.tableCellCaretOffset.value()))
                throw std::out_of_range("Invalid table cell replacement caret");
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
            if (request.tableCellCaretOffset)
                caret = caret - request.text.size() + request.tableCellCaretOffset.value();
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
                    removeTable();
                    break;
                }
                const auto promote      = rows[1u];
                const auto rowLine      = Line(nodes[row].block.source.first);
                const auto promotedLine = Line(nodes[promote].block.source.first);
                auto value              = std::wstring(Slice(nodes[promote].block.content.first, promotedLine.bodyEnd));
                const auto actual       = MarkdownTableCells(text, nodes[promote].block.content, false);
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
                Add(nodes[row].block.content.first, rowLine.bodyEnd, value);
                Add(nodes[promote].block.source.first, nodes[promote].block.source.last, L"");
                auto target       = nodes[promote].firstChild;
                const auto at     = target == MarkdownNoNode ? nodes[row].block.content.first
                                                             : nodes[row].block.content.first + (rawCellPosition(target) - nodes[promote].block.content.first);
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
                removeTable();
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
                    {
                        if (column == 0u)
                        {
                            auto first = ranges[r].first;
                            while (first < ranges[r].last && Space(text[first]))
                                ++first;
                            if (first == ranges[r].last || text[first] != L'|')
                            {
                                Add(first, first, r == 1u ? L"| --- | " : L"| | ");
                                if (r == 0u)
                                {
                                    plan.after_       = {first + 2u, first + 2u};
                                    explicitSelection = true;
                                }
                                continue;
                            }
                        }
                        Add(slots[column].first, slots[column].first, r == 1u ? L" --- |" : L"  |");
                    }
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
                    else if (column > 0u)
                        Add(slots[column - 1u].last, slots[column].last, L"");
                    else
                        Add(slots[column].first, slots[column].last, L" ");
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
        case ::DxUi::RichText::MarkdownCommand::IndentList:
        case ::DxUi::RichText::MarkdownCommand::InsertParagraph:
        case ::DxUi::RichText::MarkdownCommand::InsertTable:
        case ::DxUi::RichText::MarkdownCommand::JoinBackward:
        case ::DxUi::RichText::MarkdownCommand::JoinForward:
        case ::DxUi::RichText::MarkdownCommand::OutdentList:
        case ::DxUi::RichText::MarkdownCommand::RemoveLink:
        case ::DxUi::RichText::MarkdownCommand::SetHeading:
        case ::DxUi::RichText::MarkdownCommand::SetLink:
        case ::DxUi::RichText::MarkdownCommand::SetList:
        case ::DxUi::RichText::MarkdownCommand::ToggleStyle:
        case ::DxUi::RichText::MarkdownCommand::ToggleTask:
        case ::DxUi::RichText::MarkdownCommand::SetQuote:
        case ::DxUi::RichText::MarkdownCommand::SetCodeBlock:
        case ::DxUi::RichText::MarkdownCommand::SetCodeLanguage:
        case ::DxUi::RichText::MarkdownCommand::InsertHardBreak:
        case ::DxUi::RichText::MarkdownCommand::InsertMath:
        case ::DxUi::RichText::MarkdownCommand::InsertThematicBreak:
        case ::DxUi::RichText::MarkdownCommand::InsertImage:
        case ::DxUi::RichText::MarkdownCommand::ExitCodeBlock:
        case ::DxUi::RichText::MarkdownCommand::ClearFormatting:
        case ::DxUi::RichText::MarkdownCommand::InsertEscapedText:
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

std::wstring Detail::EscapeFileReferenceLiteral(std::wstring_view value)
{
    return LiteralMarkdown(value);
}

std::optional<Detail::FileReferenceContext> Detail::FileReferenceInsertionContext(const SourceSnapshot& source,
                                                                                  std::uint64_t revision,
                                                                                  std::shared_ptr<const MarkdownSyntax> syntax,
                                                                                  Selection selection)
{
    MarkdownCommandPlanner planner(source, revision, std::move(syntax), selection);
    // A nonempty cell's terminal ACP lies just outside its half-open source
    // range. It is safe to retain the table context only if exactly one cell
    // owns that caret; padded cells may share an ACP and need a geometry site.
    if (selection.Start() == selection.End())
    {
        std::size_t cell = MarkdownNoNode;
        for (std::size_t index = 0u; index < planner.nodes.size(); ++index)
        {
            const auto& node = planner.nodes[index];
            if (node.block.kind != Kind::TableCell || selection.caret < node.block.content.first || selection.caret > node.block.content.last)
                continue;
            if (cell != MarkdownNoNode)
                return std::nullopt;
            cell = index;
        }
        if (cell != MarkdownNoNode && planner.nodes[cell].block.content.first < planner.nodes[cell].block.content.last &&
            selection.caret == planner.nodes[cell].block.content.last)
        {
            const auto row = planner.nodes[cell].parent;
            if (row == MarkdownNoNode || planner.nodes[row].block.kind != Kind::TableRow)
                return std::nullopt;
            if (selection.caret == planner.Line(selection.caret).bodyEnd)
            {
                const auto table = planner.nodes[row].parent;
                if (table == MarkdownNoNode || planner.nodes[table].block.kind != Kind::Table)
                    return std::nullopt;
                const auto header = planner.nodes[table].firstChild;
                if (header == MarkdownNoNode || planner.nodes[header].block.kind != Kind::TableRow)
                    return std::nullopt;
                const auto cellCount = [&](std::size_t parent) noexcept
                {
                    std::size_t count{};
                    for (auto child = planner.nodes[parent].firstChild; child != MarkdownNoNode; child = planner.nodes[child].nextSibling)
                        count += planner.nodes[child].block.kind == Kind::TableCell ? 1u : 0u;
                    return count;
                };
                if (cellCount(row) < cellCount(header))
                    return std::nullopt; // Synthetic padded cells share this row-end ACP.
            }
            FileReferenceContext result;
            result.table = true;
            return result;
        }
    }
    auto leaf = planner.LeafAt(selection.Start());
    if (leaf == MarkdownNoNode)
        for (const auto kind : {Kind::FencedCode, Kind::IndentedCode, Kind::HtmlBlock, Kind::FrontMatter})
            if (const auto candidate = planner.NodeAt(kind, selection.Start()); candidate != MarkdownNoNode)
            {
                leaf = candidate;
                break;
            }
    auto anchor      = leaf;
    const auto& line = planner.Line(selection.Start());
    std::size_t content{};
    if (leaf == MarkdownNoNode)
    {
        if (selection.Start() != selection.End())
            return std::nullopt;
        for (const auto& empty : planner.plan.Syntax()->EmptyLines())
            if (empty.insertion == selection.caret)
            {
                if (anchor != MarkdownNoNode && anchor != empty.owner)
                    return std::nullopt; // Shared ACP without a unique owner is not a capability.
                anchor = empty.owner;
            }
        if (anchor == MarkdownNoNode)
        {
            for (const auto& node : planner.nodes)
                if ((node.block.kind == Kind::ListItem || node.block.kind == Kind::BlockQuote || node.block.kind == Kind::Table) &&
                    selection.Start() >= node.block.source.first && selection.Start() <= node.block.source.last)
                    return std::nullopt;
            return FileReferenceContext{};
        }
        if (anchor >= planner.nodes.size() || selection.caret < line.first || selection.caret > line.bodyEnd)
            return std::nullopt;
        content = selection.caret;
    }
    else if (selection.End() > planner.nodes[leaf].block.source.last || planner.Line(selection.Start()).first != planner.Line(selection.End()).first)
        return std::nullopt;
    if (leaf != MarkdownNoNode && planner.nodes[leaf].block.kind == Kind::TableCell)
    {
        FileReferenceContext result;
        result.table = true;
        return result;
    }
    if (leaf != MarkdownNoNode)
    {
        if (TextLeaf(planner.nodes[leaf].block.kind))
        {
            if (! planner.OuterOffset(leaf, line, content) || selection.Start() < content)
                return std::nullopt;
        }
        else
        {
            bool found = false;
            for (const auto& sourceLine : planner.nodes[leaf].lines)
                if (planner.Line(sourceLine.source.first).first == line.first)
                {
                    content = sourceLine.source.first;
                    found   = true;
                    break;
                }
            if (! found || selection.Start() < content)
                return std::nullopt;
        }
    }
    FileReferenceContext result;
    result.continuation.assign(planner.Slice(line.first, content));
    for (auto item = anchor; item != MarkdownNoNode; item = planner.nodes[item].parent)
    {
        if (planner.nodes[item].block.kind == Kind::FootnoteDefinition && planner.Line(planner.nodes[item].block.source.first).first == line.first)
        {
            std::size_t outer{};
            if (! planner.OuterOffset(item, line, outer) || planner.nodes[item].block.content.first < outer ||
                planner.nodes[item].block.content.first > content)
                return std::nullopt;
            result.continuation.replace(outer - line.first, planner.nodes[item].block.content.first - outer, planner.nodes[item].contentIndent, L' ');
            continue;
        }
        if (planner.nodes[item].block.kind != Kind::ListItem || planner.Line(planner.nodes[item].block.source.first).first != line.first)
            continue;
        ItemPrefix prefix;
        if (! planner.Prefix(item, prefix) || prefix.first < line.first || prefix.content > content)
            return std::nullopt;
        const auto column = Columns(planner.Slice(line.first, prefix.first));
        const auto width  = Columns(planner.Slice(prefix.first, prefix.content), column) - column;
        result.continuation.replace(prefix.first - line.first, prefix.content - prefix.first, width, L' ');
    }
    return result;
}

MarkdownCommandPlan BuildMarkdownCommand(const SourceSnapshot& source,
                                         std::uint64_t revision,
                                         std::shared_ptr<const MarkdownSyntax> syntax,
                                         Selection selection,
                                         const MarkdownCommandRequest& request)
{
    if (request.text.size() > Document::MaximumUnits || request.destination.size() > Document::MaximumUnits || request.title.size() > Document::MaximumUnits)
        throw std::length_error("Structural request text exceeds source capacity");
    if (static_cast<unsigned>(request.command) > static_cast<unsigned>(MarkdownCommand::InsertEscapedText))
        throw std::invalid_argument("Invalid Markdown command");
    Detail::MarkdownCommandPlanner builder(source, revision, std::move(syntax), selection);
    // Footnote labels and unresolved definitions are protected source.
    const auto note = FindCommandFootnoteOwner(*builder.plan.Syntax(), selection);
    if (! note)
        return builder.Finish();
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
        case MarkdownCommand::SetQuote: builder.Quote(request.quoteAction); break;
        case MarkdownCommand::SetCodeBlock:
        case MarkdownCommand::SetCodeLanguage: builder.CodeBlock(request); break;
        case MarkdownCommand::InsertHardBreak: builder.HardBreak(); break;
        case MarkdownCommand::InsertMath: builder.Math(request); break;
        case MarkdownCommand::InsertThematicBreak: builder.ThematicBreak(); break;
        case MarkdownCommand::InsertImage: builder.Image(request); break;
        case MarkdownCommand::ExitCodeBlock: builder.ExitCode(); break;
        case MarkdownCommand::ClearFormatting: builder.ClearFormatting(); break;
        case MarkdownCommand::InsertEscapedText: builder.EscapedText(request); break;
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
    builder.RestrictToNote(note.value());
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
