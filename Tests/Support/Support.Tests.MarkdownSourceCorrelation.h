#pragma once
#include "../../src/Controls/DxUi.MarkdownSourceCorrelation.h"
#include <DxUi/MarkdownSyntax.h>
#include <algorithm>
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
        bool rejected{};
        try
        {
            static_cast<void>(Detail::CorrelateMarkdownSource(32u, 32u, change, forbidden, forbidden));
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        check(rejected && ! reads, "invalid trusted interval bounds reject before accessing either source");
    }
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
} // namespace MarkdownSourceCorrelationTests
