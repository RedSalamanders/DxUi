#pragma once
#include "Controls/DxUi.MarkdownSourceCorrelation.h"
#include <DxUi/MarkdownSyntax.h>
#include <algorithm>
#include <format>
#include <string_view>

namespace MarkdownSourceCorrelationTests
{
using namespace DxUi::RichText;
template <typename Check> inline void Reads(Check&& check)
{
    std::size_t reads{};
    const auto forbidden = [&](std::size_t) -> wchar_t
    {
        ++reads;
        // The zero-read assertions are the oracle. A returning sentinel keeps
        // the template's legitimate string-fallback path reachable under /O2;
        // an always-throwing reader triggers MSVC's C4702 in that instantiation.
        return L'\0';
    };
    constexpr std::size_t large = 1024u * 1024u;
    for (const auto change :
         {SourceChangeRange{SourceChangeKind::TrackedEdits, 100u, 200u, 250u}, SourceChangeRange{SourceChangeKind::TrackedEdits, 10u, 800000u, 800050u}})
    {
        const auto result = Detail::CorrelateMarkdownSource(large, large + 50u, change, forbidden, forbidden);
        check(! reads && ! result.discardPrevious && result.first == change.begin && result.previousEnd == change.previousEnd &&
                  result.currentEnd == change.currentEnd,
              "trusted precise/conservative snapshot intervals require zero correlation source reads");
    }
    const auto unchanged = Detail::CorrelateMarkdownSource(large, large, SourceChangeRange{SourceChangeKind::Unchanged}, forbidden, forbidden);
    check(! reads && ! unchanged.discardPrevious && unchanged.first == large && unchanged.previousEnd == large && unchanged.currentEnd == large,
          "unchanged snapshot correlation does not scan an equal source prefix");
    const auto replacement = Detail::CorrelateMarkdownSource(large, 5u, SourceChangeRange{SourceChangeKind::FullReplacement}, forbidden, forbidden);
    check(! reads && replacement.discardPrevious && ! replacement.first && replacement.previousEnd == large && replacement.currentEnd == 5u,
          "a full replacement discards old child identities without comparing source units");
    for (const auto change : {SourceChangeRange{SourceChangeKind::TrackedEdits, 8u, 7u, 10u},
                              SourceChangeRange{SourceChangeKind::TrackedEdits, 0u, 33u, 1u},
                              SourceChangeRange{SourceChangeKind::TrackedEdits, 0u, 1u, 33u}})
    {
        const auto invalid = Detail::CorrelateMarkdownSource(32u, 32u, change, forbidden, forbidden);
        check(invalid.discardPrevious && invalid.first == 0u && invalid.previousEnd == 32u && invalid.currentEnd == 32u && ! reads,
              "invalid trusted interval bounds discard old identities without accessing either source");
    }
    const auto invalidUnchanged = Detail::CorrelateMarkdownSource(32u, 33u, SourceChangeRange{SourceChangeKind::Unchanged}, forbidden, forbidden);
    const auto unknownKind      = Detail::CorrelateMarkdownSource(32u, 33u, SourceChangeRange{static_cast<SourceChangeKind>(99)}, forbidden, forbidden);
    check(invalidUnchanged.discardPrevious && unknownKind.discardPrevious && invalidUnchanged.previousEnd == 32u && invalidUnchanged.currentEnd == 33u &&
              unknownKind.previousEnd == 32u && unknownKind.currentEnd == 33u && ! reads,
          "invalid unchanged and unknown metadata discard old identities without source reads");
    struct Example
    {
        std::wstring_view previous, current;
        std::size_t first, previousEnd, currentEnd;
    };
    constexpr Example examples[] = {{L"", L"", 0u, 0u, 0u},
                                    {L"abc", L"abc", 3u, 3u, 3u},
                                    {L"abc", L"abXc", 2u, 2u, 3u},
                                    {L"abXc", L"abc", 2u, 3u, 2u},
                                    {L"abc", L"xyz", 0u, 3u, 3u},
                                    {L"abc", L"Xabc", 0u, 0u, 1u},
                                    {L"abc", L"abcX", 3u, 3u, 4u},
                                    {L"\U0001f600", L"\U0001f601", 1u, 2u, 2u},
                                    {L"a\r\nb", L"a\nb", 1u, 2u, 1u}};
    for (const auto& example : examples)
    {
        reads               = 0u;
        const auto previous = [&](std::size_t at)
        {
            check(at < example.previous.size(), "fallback previous read is in bounds");
            ++reads;
            return example.previous[at];
        };
        const auto current = [&](std::size_t at)
        {
            check(at < example.current.size(), "fallback current read is in bounds");
            ++reads;
            return example.current[at];
        };
        const auto result = Detail::CorrelateMarkdownSource(example.previous.size(), example.current.size(), std::nullopt, previous, current);
        check(! result.discardPrevious && result.first == example.first && result.previousEnd == example.previousEnd && result.currentEnd == example.currentEnd,
              "string-only correlation retains the independently specified legacy UTF-16 prefix/suffix interval");
        check(reads <= 2u * (example.previous.size() + example.current.size() + 1u), "fallback correlation reads remain linearly bounded");
    }
}
template <typename Check> inline std::shared_ptr<const MarkdownSyntax> Complete(MarkdownParser& parser, Check&& check)
{
    std::size_t turns{};
    while (parser.HasPendingPreparation())
    {
        const auto step = parser.Advance(7u);
        check(step.workUnits > 0u && step.workUnits <= 7u && ++turns < 20000u, "correlated parser preparation retains its cooperative event allowance");
    }
    return parser.Published();
}
inline std::uint64_t Tail(const MarkdownSyntax& syntax)
{
    for (const auto& node : syntax.Nodes())
        if (node.block.kind == MarkdownBlockKind::Paragraph && syntax.Slice(node.block.content).find(L"tail") != std::wstring_view::npos)
            return node.identity;
    throw std::runtime_error("correlation identity fixture has no tail paragraph");
}
template <typename Check> inline void ParserIdentities(Check&& check)
{
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        Document document(L"# Header\r\n\r\nalpha \u03b2 \U0001f600\r\n\r\n- tail\r\n");
        MarkdownParser tracked, strings;
        tracked.Begin(document.Snapshot(), document.Revision(), profile);
        strings.Begin(document.Text(), document.Revision(), profile);
        const auto original       = Complete(tracked, check);
        const auto originalString = Complete(strings, check);
        const auto at             = document.Text().find(L"alpha");
        document.SetSelection({at, at + 5u});
        document.ReplaceSelection(L"alpha!");
        tracked.Begin(document.Snapshot(), document.Revision(), profile);
        strings.Begin(document.Text(), document.Revision(), profile);
        const auto edited       = Complete(tracked, check);
        const auto editedString = Complete(strings, check);
        check(Tail(*edited) == Tail(*original) && Tail(*editedString) == Tail(*originalString),
              "tracked and string-only edits preserve each parser's unchanged tail identity");
        check(edited->Source() == editedString->Source() && edited->Nodes().size() == editedString->Nodes().size(),
              "trusted metadata preserves the same lossless tree shape as string-only correlation");
        for (std::size_t node = 0u; node < edited->Nodes().size(); ++node)
        {
            const auto& a = edited->Nodes()[node].block;
            const auto& b = editedString->Nodes()[node].block;
            check(a.kind == b.kind && a.source == b.source && a.content == b.content && a.info == b.info,
                  "source ranges and grammar remain identical across correlation input paths");
        }
        tracked.Begin(document.Snapshot(), document.Revision() + 100u, profile);
        const auto unchanged = Complete(tracked, check);
        check(Tail(*unchanged) == Tail(*edited) && unchanged->DocumentIdentity() == edited->DocumentIdentity(),
              "same snapshot with a fresh supplied revision preserves node/document identity");
        std::uint64_t maximum{};
        for (const auto& node : unchanged->Nodes())
            maximum = std::max(maximum, node.identity);
        Document replacement(document.Text());
        tracked.Begin(replacement.Snapshot(), document.Revision() + 101u, profile);
        const auto fresh = Complete(tracked, check);
        check(fresh->Nodes().front().identity == unchanged->Nodes().front().identity && fresh->DocumentIdentity() == unchanged->DocumentIdentity(),
              "full snapshot replacement retains only the parser's document-root identity");
        for (std::size_t node = 1u; node < fresh->Nodes().size(); ++node)
            check(fresh->Nodes()[node].identity > maximum, "full replacement never reuses old child identities even for equal source spelling");
        check(original->Source().find(L"alpha!") == std::wstring_view::npos && edited->Source() == document.Text(),
              "held syntax/source publications remain immutable across correlation and replacement");
        std::wstring many;
        for (std::size_t paragraph = 0u; paragraph < 512u; ++paragraph)
            many += L"x\n\n";
        Document previousMany(many), tiny(L"new");
        tracked.Begin(previousMany.Snapshot(), 200u, profile);
        const auto manySyntax = Complete(tracked, check);
        check(manySyntax->Nodes().size() > 512u, "replacement work fixture contains independently counted old nodes");
        tracked.Begin(tiny.Snapshot(), 201u, profile);
        std::size_t work{};
        while (tracked.HasPendingPreparation())
        {
            work += tracked.Advance(1u).workUnits;
            check(work < 128u, "three-unit full replacement does not spend owner events indexing the previous 512 paragraphs");
        }
        check(tracked.Published()->Source() == L"new" && manySyntax->Source() == many,
              "bounded replacement publishes exact new source and preserves the held large tree");
    }
}
template <typename Check> inline void LexicalEdits(Check&& check)
{
    std::wstring source;
    for (unsigned line = 0u; line < 120u; ++line)
        source += L"paragraph &amp; escaped\\* and &#x41;\r\n\r\n";
    Document document(source);
    MarkdownParser incremental;
    incremental.Begin(document.Snapshot(), document.Revision(), MarkdownProfile::RichMarkdown);
    Complete(incremental, check);
    const std::wstring inserts[] = {L"&copy;", L"\\*", L"\r\n", std::wstring(L"\0", 1u), L"\U0001f600", L"&not-a-reference"};
    for (const auto& value : inserts)
    {
        auto position = document.Length() / 2u;
        while (! document.Snapshot().IsBoundary(position))
            ++position;
        document.Replace(position, position, value, {position + value.size(), position + value.size()});
        incremental.Begin(document.Snapshot(), document.Revision(), MarkdownProfile::RichMarkdown);
        const auto reused = Complete(incremental, check);
        MarkdownParser fresh;
        fresh.Begin(document.Snapshot(), document.Revision(), MarkdownProfile::RichMarkdown);
        const auto expected = Complete(fresh, check);
        check(reused && expected && reused->Source() == expected->Source() && reused->Lexemes().size() == expected->Lexemes().size(),
              "tracked lexical edits preserve the complete fresh-parser partition");
        check(reused->BlockStatistics().reusedClosedRoots > 0u && reused->Nodes().size() == expected->Nodes().size() &&
                  reused->EmptyLines().size() == expected->EmptyLines().size(),
              "separated closed root paragraphs are reused while retaining fresh-parser tree sizes");
        for (std::size_t node = 0u; node < reused->Nodes().size(); ++node)
        {
            const auto& a = reused->Nodes()[node];
            const auto& b = expected->Nodes()[node];
            check(a.block.kind == b.block.kind && a.block.source == b.block.source && a.block.content == b.block.content && a.parent == b.parent &&
                      a.firstChild == b.firstChild && a.nextSibling == b.nextSibling && a.firstInline == b.firstInline && a.lines.size() == b.lines.size(),
                  "reused root nodes match fresh block structure and source ownership");
            for (std::size_t line = 0u; line < a.lines.size(); ++line)
                check(a.lines[line].source == b.lines[line].source && a.lines[line].leadingSpaces == b.lines[line].leadingSpaces,
                      "reused root lines match fresh source mappings");
        }
        for (std::size_t index = 0u; index < reused->Lexemes().size(); ++index)
        {
            const auto& left  = reused->Lexemes()[index];
            const auto& right = expected->Lexemes()[index];
            check(left.kind == right.kind && left.source == right.source && left.valueLength == right.valueLength &&
                      reused->LexemeValue(index) == expected->LexemeValue(index),
                  "incremental lexical tokens match fresh source ranges and normalized values");
        }
    }
}
template <typename Check> inline void ClosedRootDifferential(Check&& check)
{
    struct Case
    {
        std::wstring_view source;
        MarkdownBlockKind kind;
        bool suffix;
        bool expectReuse;
    };
    const Case cases[] = {
        {L"paragraph *text*\n", MarkdownBlockKind::Paragraph, true, true},
        {L"paragraph [label][missing] with reference\r\n", MarkdownBlockKind::Paragraph, true, true},
        {L"```cpp\nopen\n", MarkdownBlockKind::FencedCode, true, false},
        {L"# heading **bold**\n", MarkdownBlockKind::AtxHeading, true, true},
        {L"heading\n===\n", MarkdownBlockKind::SetextHeading, true, true},
        {L"***\n", MarkdownBlockKind::ThematicBreak, true, true},
        {L"```cpp\none\ntwo\n```\n", MarkdownBlockKind::FencedCode, true, true},
        {L"    one\n    two\n", MarkdownBlockKind::IndentedCode, true, true},
        {L"<div>\none\n</div>\n", MarkdownBlockKind::HtmlBlock, true, true},
        {L"> one\n> - nested\n>   text\n", MarkdownBlockKind::BlockQuote, true, true},
        {L"- one\n  - nested\n\n  tail\n- two\n", MarkdownBlockKind::List, true, true},
        {L"| a | b |\n| - | - |\n| c | d |\n", MarkdownBlockKind::Table, true, true},
        {L"[^note]: first\n    continuation\n", MarkdownBlockKind::FootnoteDefinition, true, true},
        {L"[key]: /target \"title\"\n", MarkdownBlockKind::ReferenceDefinition, true, false},
        {L"---\nname: value\n---\n", MarkdownBlockKind::FrontMatter, false, true},
    };
    const auto equal = [&](const MarkdownSyntax& a, const MarkdownSyntax& b)
    {
        check(a.Source() == b.Source() && a.Nodes().size() == b.Nodes().size() && a.Blocks().size() == b.Blocks().size() &&
                  a.EmptyLines().size() == b.EmptyLines().size() && a.Inlines().size() == b.Inlines().size() && a.Lexemes().size() == b.Lexemes().size() &&
                  a.Diagnostics().size() == b.Diagnostics().size() && a.Footnotes().size() == b.Footnotes().size() &&
                  a.FootnoteReferences().size() == b.FootnoteReferences().size(),
              "incremental closed roots match fresh publication sizes");
        const auto equalBlock = [](const auto& x, const auto& y)
        {
            return x.kind == y.kind && x.source == y.source && x.content == y.content && x.info == y.info && x.heading == y.heading &&
                   x.fenceIndent == y.fenceIndent && x.fenceMarker == y.fenceMarker && x.fenceLength == y.fenceLength && x.closed == y.closed;
        };
        for (size_t i = 0u; i < a.Blocks().size(); ++i)
            check(equalBlock(a.Blocks()[i], b.Blocks()[i]), "incremental root block metadata matches fresh parse");
        for (size_t i = 0u; i < a.Nodes().size(); ++i)
        {
            const auto& x = a.Nodes()[i];
            const auto& y = b.Nodes()[i];
            check(equalBlock(x.block, y.block) && x.identity == y.identity && x.parent == y.parent && x.firstChild == y.firstChild &&
                      x.nextSibling == y.nextSibling && x.firstInline == y.firstInline && x.label == y.label && x.destination == y.destination &&
                      x.title == y.title && x.listStart == y.listStart && x.contentIndent == y.contentIndent && x.htmlType == y.htmlType &&
                      x.listMarker == y.listMarker && x.tight == y.tight && x.alignment == y.alignment && x.task == y.task && x.tableHeader == y.tableHeader &&
                      x.frontMatterKind == y.frontMatterKind && x.lines.size() == y.lines.size(),
                  "incremental subtree metadata, links and reconciled identities match fresh parse");
            for (size_t line = 0u; line < x.lines.size(); ++line)
                check(x.lines[line].source == y.lines[line].source && x.lines[line].leadingSpaces == y.lines[line].leadingSpaces,
                      "incremental descendant lines retain exact source and virtual indentation");
        }
        for (size_t i = 0u; i < a.EmptyLines().size(); ++i)
        {
            const auto& x = a.EmptyLines()[i];
            const auto& y = b.EmptyLines()[i];
            check(x.source == y.source && x.insertion == y.insertion && x.owner == y.owner && x.indent == y.indent,
                  "incremental empty-line ownership matches fresh parse");
        }
        for (size_t i = 0u; i < a.Inlines().size(); ++i)
        {
            const auto& x = a.Inlines()[i];
            const auto& y = b.Inlines()[i];
            check(x.kind == y.kind && x.source == y.source && x.firstChild == y.firstChild && x.nextSibling == y.nextSibling && x.value == y.value &&
                      x.destination == y.destination && x.title == y.title && x.filteredHtmlOpeners == y.filteredHtmlOpeners,
                  "incremental inline and reference resolutions match fresh parse");
        }
        for (size_t i = 0u; i < a.Lexemes().size(); ++i)
            check(a.Lexemes()[i].kind == b.Lexemes()[i].kind && a.Lexemes()[i].source == b.Lexemes()[i].source && a.LexemeValue(i) == b.LexemeValue(i),
                  "incremental lexical partition matches fresh parse");
        for (size_t i = 0u; i < a.Diagnostics().size(); ++i)
            check(a.Diagnostics()[i].code == b.Diagnostics()[i].code && a.Diagnostics()[i].source == b.Diagnostics()[i].source &&
                      a.Diagnostics()[i].node == b.Diagnostics()[i].node,
                  "incremental diagnostics match fresh parse");
        for (size_t i = 0u; i < a.Footnotes().size(); ++i)
            check(a.Footnotes()[i].label == b.Footnotes()[i].label && a.Footnotes()[i].definition == b.Footnotes()[i].definition &&
                      a.Footnotes()[i].ordinal == b.Footnotes()[i].ordinal && a.Footnotes()[i].references == b.Footnotes()[i].references,
                  "incremental footnote definitions and occurrence order match fresh parse");
        for (size_t i = 0u; i < a.FootnoteReferences().size(); ++i)
            check(a.FootnoteReferences()[i].inlineNode == b.FootnoteReferences()[i].inlineNode &&
                      a.FootnoteReferences()[i].footnote == b.FootnoteReferences()[i].footnote &&
                      a.FootnoteReferences()[i].occurrence == b.FootnoteReferences()[i].occurrence,
                  "incremental footnote references match fresh parse");
    };
    for (const auto& item : cases)
        for (const bool suffix : {false, true})
        {
            if (suffix && ! item.suffix)
                continue;
            const auto source = suffix ? std::wstring(L"TARGET\n\n") + std::wstring(item.source) + L"\n" : std::wstring(item.source) + L"\nTARGET\n";
            Document document(source);
            MarkdownParser tracked, full;
            tracked.Begin(document.Snapshot(), document.Revision(), MarkdownProfile::RichMarkdown);
            full.Begin(document.Text(), document.Revision(), MarkdownProfile::RichMarkdown);
            const auto old = Complete(tracked, check);
            static_cast<void>(Complete(full, check));
            check(std::ranges::any_of(old->Nodes(), [&](const auto& node) { return node.parent == 0u && node.block.kind == item.kind; }),
                  "closed-root fixture contains its intended root kind");
            const auto at = source.find(L"TARGET");
            document.Replace(at, at + 6u, L"TARGET!", {at + 7u, at + 7u});
            tracked.Begin(document.Snapshot(), document.Revision(), MarkdownProfile::RichMarkdown);
            full.Begin(document.Text(), document.Revision(), MarkdownProfile::RichMarkdown);
            const auto changed  = Complete(tracked, check);
            const auto expected = Complete(full, check);
            equal(*changed, *expected);
            const auto description = std::format("closed root kind {} suffix {} should reuse", static_cast<unsigned>(item.kind), suffix);
            if (item.expectReuse &&
                ((item.kind != MarkdownBlockKind::IndentedCode && item.kind != MarkdownBlockKind::List && item.kind != MarkdownBlockKind::FootnoteDefinition) ||
                 suffix))
                check(changed->BlockStatistics().reusedClosedRoots > 0u, description.c_str());
            else
                check(changed->BlockStatistics().reusedClosedRoots == 0u, "unproven closure and reference-definition roots retain safe full parsing");
        }
}
template <typename Check> inline void ClosedRootCancellation(Check&& check)
{
    const std::wstring source = L"TARGET\n\n- alpha\n  - beta\n    - gamma\n\n  tail\n";
    Document document(source);
    MarkdownParser parser;
    parser.Begin(document.Snapshot(), document.Revision(), MarkdownProfile::RichMarkdown);
    const auto published = Complete(parser, check);
    const auto at        = source.find(L"TARGET");
    document.Replace(at, at + 6u, L"TARGET!", {at + 7u, at + 7u});
    parser.Begin(document.Snapshot(), document.Revision(), MarkdownProfile::RichMarkdown);
    for (unsigned turn = 0u; turn < 20u; ++turn)
    {
        const auto advance = parser.Advance(1u);
        check(advance.workUnits == 1u && parser.Published() == published && published->Source() == source,
              "incremental subtree preparation is budgeted and retains prior publication");
    }
    parser.Cancel();
    check(parser.Published() == published && published->Source() == source, "cancelling subtree preparation retains the published tree");
    parser.Begin(document.Snapshot(), document.Revision(), MarkdownProfile::RichMarkdown);
    const auto recovered = Complete(parser, check);
    check(recovered && recovered->Source() == document.Text() && recovered->BlockStatistics().reusedClosedRoots > 0u,
          "cancelled subtree preparation retries and reuses the unchanged subtree");
}
} // namespace MarkdownSourceCorrelationTests
