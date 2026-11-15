#pragma once
// W7.2 incremental parse reuse: the AST-equality oracle. An incremental MarkdownParser, fed tracked snapshots (and a second
// one fed strings), must publish exactly the tree a brand-new parser publishes for the same text: blocks, nodes and their
// lines, empty lines, lexemes, inlines with their resolved links, diagnostics (front matter included) and footnotes. Node
// identities are the one revision-local difference; their lineage is checked separately. Shared by the EditorControls suite.
#include "Support/WorkCounters.h"
#include <DxUi/MarkdownSyntax.h>
#include <DxUi/RichDocument.h>
#include <array>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace MarkdownParseReuseTests
{
using namespace DxUi::RichText;

template <typename Check> inline std::shared_ptr<const MarkdownSyntax> Complete(MarkdownParser& parser, Check&& check)
{
    while (parser.HasPendingPreparation())
        check(parser.Advance(1u << 16u).status != MarkdownPreparationStatus::Failed, "an oracle parse completes within the node capacity");
    return parser.Published();
}

// The first difference between two publications of the same text, or an empty string when they are equal in everything
// but node identity and the work statistics.
inline std::string FirstDifference(const MarkdownSyntax& a, const MarkdownSyntax& b)
{
    if (a.Source() != b.Source())
        return "source";
    if (a.Profile() != b.Profile() || a.Extensions() != b.Extensions())
        return "profile";
    const auto sameBlock = [](const MarkdownBlock& x, const MarkdownBlock& y)
    {
        return x.kind == y.kind && x.source == y.source && x.content == y.content && x.info == y.info && x.heading == y.heading &&
               x.fenceIndent == y.fenceIndent && x.fenceMarker == y.fenceMarker && x.fenceLength == y.fenceLength && x.closed == y.closed;
    };
    if (a.Blocks().size() != b.Blocks().size())
        return std::format("root block count {} vs {}", a.Blocks().size(), b.Blocks().size());
    for (std::size_t i = 0u; i < a.Blocks().size(); ++i)
        if (! sameBlock(a.Blocks()[i], b.Blocks()[i]))
            return std::format("root block {}", i);
    if (a.Nodes().size() != b.Nodes().size())
        return std::format("node count {} vs {}", a.Nodes().size(), b.Nodes().size());
    for (std::size_t i = 0u; i < a.Nodes().size(); ++i)
    {
        const auto& x = a.Nodes()[i];
        const auto& y = b.Nodes()[i];
        if (! sameBlock(x.block, y.block) || x.parent != y.parent || x.firstChild != y.firstChild || x.nextSibling != y.nextSibling || x.label != y.label ||
            x.destination != y.destination || x.title != y.title || x.listStart != y.listStart || x.contentIndent != y.contentIndent ||
            x.htmlType != y.htmlType || x.listMarker != y.listMarker || x.tight != y.tight || x.firstInline != y.firstInline || x.alignment != y.alignment ||
            x.task != y.task || x.tableHeader != y.tableHeader || x.frontMatterKind != y.frontMatterKind || x.lines.size() != y.lines.size())
            return std::format("node {} (kind {})", i, static_cast<unsigned>(x.block.kind));
        for (std::size_t line = 0u; line < x.lines.size(); ++line)
            if (x.lines[line].source != y.lines[line].source || x.lines[line].leadingSpaces != y.lines[line].leadingSpaces)
                return std::format("node {} line {}", i, line);
    }
    for (std::size_t i = 0u; i < (std::min)(a.EmptyLines().size(), b.EmptyLines().size()); ++i)
    {
        const auto& x = a.EmptyLines()[i];
        const auto& y = b.EmptyLines()[i];
        if (x.source != y.source || x.insertion != y.insertion || x.owner != y.owner || x.indent != y.indent)
            return std::format("empty line {} [{}, {}) owner {} vs [{}, {}) owner {} (counts {} vs {})",
                               i,
                               x.source.first,
                               x.source.last,
                               x.owner,
                               y.source.first,
                               y.source.last,
                               y.owner,
                               a.EmptyLines().size(),
                               b.EmptyLines().size());
    }
    if (a.EmptyLines().size() != b.EmptyLines().size())
        return std::format("empty line count {} vs {}", a.EmptyLines().size(), b.EmptyLines().size());
    if (a.Lexemes().size() != b.Lexemes().size())
        return std::format("lexeme count {} vs {}", a.Lexemes().size(), b.Lexemes().size());
    for (std::size_t i = 0u; i < a.Lexemes().size(); ++i)
        if (a.Lexemes()[i].kind != b.Lexemes()[i].kind || a.Lexemes()[i].source != b.Lexemes()[i].source || a.LexemeValue(i) != b.LexemeValue(i))
            return std::format("lexeme {}", i);
    if (a.Inlines().size() != b.Inlines().size())
        return std::format("inline count {} vs {}", a.Inlines().size(), b.Inlines().size());
    for (std::size_t i = 0u; i < a.Inlines().size(); ++i)
    {
        const auto& x = a.Inlines()[i];
        const auto& y = b.Inlines()[i];
        if (x.kind != y.kind || x.source != y.source || x.firstChild != y.firstChild || x.nextSibling != y.nextSibling || x.value != y.value ||
            x.destination != y.destination || x.title != y.title || x.filteredHtmlOpeners != y.filteredHtmlOpeners || x.wiki != y.wiki)
            return std::format("inline {} (kind {})", i, static_cast<unsigned>(x.kind));
    }
    if (a.Diagnostics().size() != b.Diagnostics().size())
        return std::format("diagnostic count {} vs {}", a.Diagnostics().size(), b.Diagnostics().size());
    for (std::size_t i = 0u; i < a.Diagnostics().size(); ++i)
        if (a.Diagnostics()[i].code != b.Diagnostics()[i].code || a.Diagnostics()[i].source != b.Diagnostics()[i].source ||
            a.Diagnostics()[i].node != b.Diagnostics()[i].node)
            return std::format("diagnostic {}", i);
    if (a.Footnotes().size() != b.Footnotes().size() || a.FootnoteReferences().size() != b.FootnoteReferences().size())
        return "footnote count";
    for (std::size_t i = 0u; i < a.Footnotes().size(); ++i)
        if (a.Footnotes()[i].label != b.Footnotes()[i].label || a.Footnotes()[i].definition != b.Footnotes()[i].definition ||
            a.Footnotes()[i].ordinal != b.Footnotes()[i].ordinal || a.Footnotes()[i].references != b.Footnotes()[i].references)
            return std::format("footnote {}", i);
    for (std::size_t i = 0u; i < a.FootnoteReferences().size(); ++i)
        if (a.FootnoteReferences()[i].inlineNode != b.FootnoteReferences()[i].inlineNode ||
            a.FootnoteReferences()[i].footnote != b.FootnoteReferences()[i].footnote ||
            a.FootnoteReferences()[i].occurrence != b.FootnoteReferences()[i].occurrence)
            return std::format("footnote reference {}", i);
    if (a.SourceRegionNode() != b.SourceRegionNode())
        return "source region node";
    return {};
}

// Identity lineage of a tracked incremental parse: identities stay unique, and a top-level root that lies wholly before
// or wholly after the edit, with the same kind and (shifted) range as an old root, keeps the old root's identity.
inline std::string IdentityDifference(const MarkdownSyntax& old, const MarkdownSyntax& changed, const SourceChangeRange& change)
{
    std::set<std::uint64_t> seen;
    for (const auto& node : changed.Nodes())
        if (! seen.insert(node.identity).second)
            return "duplicate node identity";
    if (change.kind != SourceChangeKind::TrackedEdits)
        return {};
    for (const auto& node : changed.Nodes())
    {
        if (node.parent != 0u)
            continue;
        MarkdownRange oldRange{};
        if (node.block.source.last < change.begin)
            oldRange = node.block.source;
        else if (node.block.source.first > change.currentEnd)
            oldRange = {node.block.source.first - change.currentEnd + change.previousEnd, node.block.source.last - change.currentEnd + change.previousEnd};
        else
            continue;
        for (const auto& candidate : old.Nodes())
            if (candidate.parent == 0u && candidate.block.kind == node.block.kind && candidate.block.source == oldRange &&
                candidate.block.content.last - candidate.block.content.first == node.block.content.last - node.block.content.first &&
                candidate.identity != node.identity)
                return std::format("unchanged root at {} lost its identity", node.block.source.first);
    }
    return {};
}

// ---- Seeded edit fuzz -------------------------------------------------------------------------------------------------

// Block pieces a fuzz document is assembled from; each also serves as a multi-line paste.
inline constexpr std::array<std::wstring_view, 22> kPieces{
    L"# Heading *one* with [ref]\n\n",
    L"Paragraph with [ref], [text][ref2], *em*, **strong**, `code`, <span>raw</span>, https://example.com/x and [[Wiki|label]].\n"
    L"Lazy second line with $x^2$ math and a note[^n1].\n\n",
    L"> quote line with *emphasis\nlazy continuation line*\n> - nested item\n> more\n\n",
    L"- item one\n- item two\nlazy item text\n\n  second paragraph\n  - nested\n1. ordered\n\n",
    L"[ref]: /url \"Title\"\n[ref2]: <other target> 'Second'\n\n",
    L"```js\ncode **not** [ref]\n```\n\n",
    L"~~~\nopen tilde fence\n~~~\n\n",
    L"Setext heading\n---\n\nAnother [ref2]\n===\n\n",
    L"<div>\nhtml *block*\n</div>\n\n",
    L"| a | b |\n| - | :-: |\n| c [ref] | d |\n| e \\| f | `g` |\n\n",
    L"    indented code\n    more code\n\n",
    L"[^n1]: Footnote text with [ref]\n    continued\n\n",
    L"Term\n: Definition of the term\n\n",
    L"*[HTML]: Hyper Text Markup Language\n\nHTML is used here.\n\n",
    L"$$\nx = \\frac{a}{b}\n$$\n\n",
    L"***\n\n",
    L"Final paragraph &amp; &copy; \\* ~~struck~~ ==marked== ^sup^ ~sub~ ++ins++ end.\n",
    L"Text right before a fence\n```\nfenced without a blank\n```\nText right after it\n",
    L"1. one\n2. two\n\n   indented\n3. three\n",
    L"<!-- comment\nspanning -->\nafter comment\n\n",
    L"A line\n    not code (lazy)\n> quote\ncontinued lazily\n\n",
    L"[ref]: /redefined\n\n",
};
inline constexpr std::array<std::wstring_view, 3> kFrontMatter{
    L"---\ntitle: Fuzz\ntags: [a, b]\n---\n",
    L"---\nkey: 1\nkey: 2\n---\n\n",
    L"+++\ntitle = \"Fuzz\"\n+++\n\n",
};
inline constexpr std::array<std::wstring_view, 45> kTokens{
    L"a",          L" ",    L"*",       L"_",      L"`",     L"[",     L"]",  L"|",     L"-",       L"=",      L">",
    L"#",          L"\n",   L"\r\n",    L"$",      L"~",     L"<",     L"&",  L"\\",    L":",       L"^",      L"!",
    L"```",        L"~~~",  L"---",     L"+++",    L"===",   L"> ",    L"- ", L"1. ",   L"    ",    L"[ref]",  L"[ref]: /new\n",
    L"[^n1]",      L"\n\n", L"<div>\n", L"</div>", L"| x |", L"| - |", L"$$", L"---\n", L"\n---\n", L"[x]: <", L"*[x]: y\n",
    L"[^n2]: z\n",
};

struct FuzzResult
{
    std::size_t edits{}, compared{}, inlineFullReparses{}, leavesCopied{}, leavesParsed{}, frontMatterReused{}, frontMatterValidated{};
    std::size_t frontMatterCreated{}, frontMatterRemoved{}, fencesOpened{}, fencesClosed{}, referenceChanges{}, lazyDocuments{}, setextChanges{};
    std::size_t atStart{}, atEnd{}, deletions{}, pastes{};
};

inline std::size_t CountKind(const MarkdownSyntax& syntax, MarkdownBlockKind kind, bool closedOnly = false)
{
    std::size_t count{};
    for (const auto& node : syntax.Nodes())
        if (node.block.kind == kind && (! closedOnly || node.block.closed))
            ++count;
    return count;
}
inline bool HasFrontMatter(const MarkdownSyntax& syntax)
{
    return syntax.Nodes().size() > 1u && syntax.Nodes()[0].firstChild != MarkdownNoNode &&
           syntax.Nodes()[syntax.Nodes()[0].firstChild].block.kind == MarkdownBlockKind::FrontMatter;
}
// A paragraph line inside a block quote or list item whose physical line does not carry the container's prefix.
inline bool HasLazyLine(const MarkdownSyntax& syntax)
{
    const auto source = syntax.Source();
    for (const auto& node : syntax.Nodes())
    {
        if (node.block.kind != MarkdownBlockKind::Paragraph || node.parent == MarkdownNoNode || node.parent == 0u)
            continue;
        const auto parent = syntax.Nodes()[node.parent].block.kind;
        if (parent != MarkdownBlockKind::BlockQuote && parent != MarkdownBlockKind::ListItem)
            continue;
        for (std::size_t line = 1u; line < node.lines.size(); ++line)
        {
            auto start = node.lines[line].source.first;
            while (start > 0u && source[start - 1u] != L'\n' && source[start - 1u] != L'\r')
                --start;
            if (start == node.lines[line].source.first)
                return true;
        }
    }
    return false;
}

// An ASCII rendering of a failing fuzz document, so the report reproduces it.
inline std::string Escape(std::wstring_view text)
{
    std::string result;
    for (const auto c : text)
        if (c == L'\n')
            result += "\\n";
        else if (c == L'\r')
            result += "\\r";
        else if (c == L'\\')
            result += "\\\\";
        else if (c >= 0x20 && c < 0x7f)
            result += static_cast<char>(c);
        else
            result += std::format("\\u{:04X}", static_cast<unsigned>(c));
    return result;
}

template <typename Check> inline FuzzResult RunEditFuzz(std::uint64_t seed, std::size_t edits, MarkdownProfile profile, Check&& check)
{
    std::mt19937_64 random(seed);
    const auto pick = [&](std::size_t count) { return std::uniform_int_distribution<std::size_t>(0u, count - 1u)(random); };
    // Documents of a few thousand units keep 20,000 compared publications affordable; the edits still reach every construct.
    std::wstring text;
    const bool crlf = pick(4u) == 0u;
    if (profile == MarkdownProfile::RichMarkdown && pick(2u) == 0u)
        text += kFrontMatter[pick(kFrontMatter.size())];
    for (std::size_t piece = 0u, count = 8u + pick(10u); piece < count; ++piece)
        text += kPieces[pick(kPieces.size())];
    if (crlf)
    {
        std::wstring converted;
        for (const auto c : text)
            converted += c == L'\n' ? std::wstring(L"\r\n") : std::wstring(1u, c);
        text = converted;
    }
    Document document(text);
    MarkdownParser tracked, strings;
    tracked.Begin(document.Snapshot(), document.Revision(), profile);
    strings.Begin(document.Text(), document.Revision(), profile);
    auto previous = Complete(tracked, check);
    static_cast<void>(Complete(strings, check));
    FuzzResult result;
    for (std::size_t edit = 0u; edit < edits; ++edit)
    {
        const auto current = document.Text();
        const auto length  = current.size();
        std::size_t first{}, last{};
        const auto where = pick(10u);
        first            = where == 0u ? 0u : where == 1u ? length : pick(length + 1u);
        const auto kind  = length > 6000u ? 19u : pick(20u);
        std::wstring insert;
        if (kind < 11u)
            insert = kTokens[pick(kTokens.size())];
        else if (kind < 13u)
        {
            insert = kPieces[pick(kPieces.size())];
            if (pick(3u) == 0u)
                insert = std::wstring(insert.substr(0u, pick(insert.size()) + 1u));
            ++result.pastes;
        }
        else if (kind < 15u)
            insert = kFrontMatter[pick(kFrontMatter.size())];
        if (kind >= 15u || kind == 1u || (kind < 11u && pick(4u) == 0u))
        {
            last = first + 1u + pick(kind >= 18u ? 120u : 12u);
            ++result.deletions;
        }
        last = std::min(last, length);
        if (last < first)
            last = first;
        while (first > 0u && ! IsEditBoundary(current, first))
            --first;
        while (last < length && ! IsEditBoundary(current, last))
            ++last;
        if (length - (last - first) + insert.size() > Document::MaximumUnits)
            continue;
        bool changed{};
        const auto before = document.Snapshot();
        if (document.TryReplace(first, last, insert, {first + insert.size(), first + insert.size()}, changed) != SourceResult::Success || ! changed)
            continue;
        result.atStart += first == 0u;
        result.atEnd += last == length;
        const auto change = document.Snapshot().ChangedRangeFrom(before);
        ++result.edits;
        DxUi::Detail::WorkCounterScope scope;
        tracked.Begin(document.Snapshot(), document.Revision(), profile);
        const auto incremental = Complete(tracked, check);
        const auto work        = scope.Delta().rich;
        strings.Begin(document.Text(), document.Revision(), profile);
        const auto stringIncremental = Complete(strings, check);
        MarkdownParser fresh;
        fresh.Begin(document.Text(), document.Revision(), profile);
        const auto expected         = Complete(fresh, check);
        const auto difference       = FirstDifference(*incremental, *expected);
        const auto stringDifference = FirstDifference(*stringIncremental, *expected);
        if (! difference.empty() || ! stringDifference.empty())
            std::cout << "  previous source: " << Escape(previous->Source()) << "\n  edited source: " << Escape(document.Text()) << "\n"
                      << std::format("  parse reuse fuzz seed 0x{:X} edit {} at [{}, {}) inserting {} units: {}{}\n",
                                     seed,
                                     edit,
                                     first,
                                     last,
                                     insert.size(),
                                     difference.empty() ? "" : "tracked: " + difference + " ",
                                     stringDifference.empty() ? "" : "strings: " + stringDifference);
        check(difference.empty(), "an incremental parse publishes the tree a full parse publishes (tracked snapshots)");
        check(stringDifference.empty(), "an incremental parse publishes the tree a full parse publishes (strings)");
        const auto identity = IdentityDifference(*previous, *incremental, change);
        if (! identity.empty())
            std::cout << std::format("  parse reuse fuzz seed 0x{:X} edit {}: {}\n", seed, edit, identity);
        check(identity.empty(), "incremental parse identities stay unique and unchanged roots keep theirs");
        ++result.compared;
        result.inlineFullReparses += work.markdownInlineFullReparses;
        result.leavesCopied += work.markdownInlineLeavesCopied;
        result.leavesParsed += work.markdownInlineLeavesParsed;
        result.frontMatterReused += work.markdownFrontMatterValidationsReused;
        result.frontMatterValidated += work.markdownFrontMatterValidations;
        const bool hadFrontMatter = HasFrontMatter(*previous), hasFrontMatter = HasFrontMatter(*incremental);
        result.frontMatterCreated += ! hadFrontMatter && hasFrontMatter;
        result.frontMatterRemoved += hadFrontMatter && ! hasFrontMatter;
        const auto fencesBefore = CountKind(*previous, MarkdownBlockKind::FencedCode), fencesAfter = CountKind(*incremental, MarkdownBlockKind::FencedCode);
        const auto closedBefore = CountKind(*previous, MarkdownBlockKind::FencedCode, true),
                   closedAfter  = CountKind(*incremental, MarkdownBlockKind::FencedCode, true);
        result.fencesOpened += fencesAfter - closedAfter > fencesBefore - closedBefore;
        result.fencesClosed += closedAfter > closedBefore && fencesAfter == fencesBefore;
        result.referenceChanges +=
            CountKind(*previous, MarkdownBlockKind::ReferenceDefinition) != CountKind(*incremental, MarkdownBlockKind::ReferenceDefinition) ||
            work.markdownInlineFullReparses != 0u;
        result.setextChanges += CountKind(*previous, MarkdownBlockKind::SetextHeading) != CountKind(*incremental, MarkdownBlockKind::SetextHeading);
        result.lazyDocuments += HasLazyLine(*incremental);
        previous = incremental;
    }
    return result;
}
} // namespace MarkdownParseReuseTests
