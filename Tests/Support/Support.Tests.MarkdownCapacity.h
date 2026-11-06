#pragma once
#include "Controls/DxUi.MarkdownBlocks.h"
#include "Support.Tests.MarkdownHtml.h"
#include <DxUi/MarkdownSyntax.h>

namespace MarkdownCapacityTests
{
using namespace DxUi::RichText;
inline std::wstring Table(std::size_t columns, std::size_t rows)
{
    std::wstring source(0u, L' ');
    for (std::size_t column = 0u; column < columns; ++column)
        source.append(L"a|");
    source.push_back(L'\n');
    for (std::size_t column = 0u; column < columns; ++column)
        source.append(L"---|");
    source.push_back(L'\n');
    for (std::size_t row = 0u; row < rows; ++row)
        source.append(L"|x|\n");
    return std::wstring(source);
}
template <typename Check> inline std::shared_ptr<const MarkdownSyntax> Complete(MarkdownParser& parser, Check&& check)
{
    std::size_t work{};
    while (parser.HasPendingPreparation())
    {
        const auto step = parser.Advance(113u);
        work += step.workUnits;
        check(step.workUnits > 0u && step.workUnits <= 113u && work < (Document::MaximumUnits + MarkdownSyntax::MaximumNodeRecords) * 32u,
              "bounded syntax admission yields and completes without spinning an owner");
    }
    return parser.Published();
}
template <typename Check> inline void AutolinkSuffixes(Check&& check)
{
    struct Case
    {
        std::wstring_view source, expected;
    };
    constexpr Case cases[] = {
        {L"Visit www.a.com today, **not** www.b.com.",
         L"<p>Visit <a href=\"http://www.a.com\">www.a.com</a> today, <strong>not</strong> <a href=\"http://www.b.com\">www.b.com</a>.</p>\n"},
        {L"see www.a.com for snake_case and www.b.com",
         L"<p>see <a href=\"http://www.a.com\">www.a.com</a> for snake_case and <a href=\"http://www.b.com\">www.b.com</a></p>\n"},
        {L"a https://x.example and **https://y.example**",
         L"<p>a <a href=\"https://x.example\">https://x.example</a> and <strong><a href=\"https://y.example\">https://y.example</a></strong></p>\n"},
        {L"www.a.com x [y www.b.com", L"<p><a href=\"http://www.a.com\">www.a.com</a> x [y <a href=\"http://www.b.com\">www.b.com</a></p>\n"},
        {L"www.a.com is fast = good, www.b.com",
         L"<p><a href=\"http://www.a.com\">www.a.com</a> is fast = good, <a href=\"http://www.b.com\">www.b.com</a></p>\n"},
        {L"www.a.com x **y** www.b.com",
         L"<p><a href=\"http://www.a.com\">www.a.com</a> x <strong>y</strong> <a href=\"http://www.b.com\">www.b.com</a></p>\n"},
    };
    for (const auto profile : {MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
        for (const auto& item : cases)
        {
            MarkdownParser parser;
            parser.Begin(item.source, 1u, profile);
            const auto syntax = Complete(parser, check);
            check(syntax->Source() == item.source && MarkdownHtmlTests::Html(*syntax) == item.expected,
                  "autolink siblings retain every literal suffix in independently specified HTML");
            for (const auto& node : syntax->Inlines())
                check(node.kind != MarkdownInlineKind::Text || node.source.first == node.source.last || ! node.value.empty(),
                      "nonempty text between autolinks has a decoded value");
            check(syntax->InlineStatistics().autolinkDecodedUnits <= item.source.size(), "each autolink literal source unit is decoded at most once");
        }
}
template <typename Check> inline void ParserBounds(Check&& check)
{
    MarkdownParser parser;
    parser.Begin(L"old", 1u, MarkdownProfile::Gfm029);
    const auto old = Complete(parser, check);
    for (const std::size_t rows : {10920u, 10921u})
    {
        const auto source = Table(1u, rows);
        parser.Begin(source, 2u + rows, MarkdownProfile::Gfm029);
        const auto syntax = Complete(parser, check);
        check(syntax->Source() == source && syntax->Nodes().size() + syntax->Inlines().size() == 5u + 3u * rows &&
                  syntax->Nodes().size() + syntax->Inlines().size() <= MarkdownSyntax::MaximumNodeRecords,
              "independent one-column table counts admit the exact combined node boundary");
    }
    auto global = Table(100u, 200u);
    global.append(L"\n").append(Table(100u, 200u));
    std::wstring dense(0u, L' ');
    for (std::size_t i = 0u; i < 12000u; ++i)
        dense.append(L"*x* ");
    auto html = Table(1u, 10920u);
    html.append(L"\n<hr>\n\n<hr>\n");
    std::wstring references(0u, L' ');
    for (std::size_t i = 0u; i < 10921u; ++i)
        references.append(L"- -\n");
    for (std::size_t i = 0u; i < 3641u; ++i)
        references.append(L"[a]:b\n");
    const std::wstring rejected[] = {Table(100u, 400u), Table(1u, 10922u), global, dense, html, references};
    for (const auto& source : rejected)
    {
        check(source.size() <= Document::MaximumUnits, "amplification fixtures fit source admission before exercising the independent node limit");
        parser.Begin(L"old", 1u, MarkdownProfile::Gfm029);
        Complete(parser, check);
        const auto previous = parser.Published();
        parser.Begin(source, 3u, MarkdownProfile::Gfm029);
        MarkdownAdvance failed{};
        while (parser.HasPendingPreparation())
        {
            failed = parser.Advance(113u);
            if (failed.status == MarkdownPreparationStatus::Failed)
                break;
        }
        check(failed.status == MarkdownPreparationStatus::Failed && failed.result == MarkdownResult::CapacityExceeded && parser.Published() == previous &&
                  ! parser.HasPendingPreparation() && previous->Source() == L"old" && old->Source() == L"old",
              "table/global/inline/HTML capacity failure retires staging without publishing or changing held source");
        parser.Begin(L"retry", 4u, MarkdownProfile::Gfm029);
        check(Complete(parser, check)->Source() == L"retry", "a parser can explicitly retry after every capacity failure");
    }
    const auto pending = Table(1000u, 1u);
    std::vector<MarkdownNode> stagedNodes(0u);
    std::vector<MarkdownEmptyLine> stagedEmptyLines(0u);
    Detail::MarkdownBlocks blocks(pending, MarkdownProfile::Gfm029, stagedNodes, stagedEmptyLines, MarkdownExtensions::All);
    const auto headerEnd = pending.find(L'\n');
    const auto ruleEnd   = pending.find(L'\n', headerEnd + 1u);
    blocks.Line(0u, headerEnd, headerEnd + 1u);
    blocks.Line(headerEnd + 1u, ruleEnd, ruleEnd + 1u);
    for (std::size_t cell = 0u; cell < 128u; ++cell)
    {
        const auto before = stagedNodes.size();
        check(blocks.AdvanceTableCell() && stagedNodes.size() == before + 1u && stagedNodes.back().block.kind == MarkdownBlockKind::TableCell,
              "a pending header produces exactly one table cell per block work unit");
    }
    parser.Begin(pending, 5u, MarkdownProfile::Gfm029);
    for (std::size_t step = 0u; step < 128u; ++step)
        check(parser.Advance(1u).workUnits == 1u && parser.Published()->Source() == L"retry",
              "parser preparation yields without exposing its incomplete candidate");
    parser.Cancel();
    check(parser.Published()->Source() == L"retry" && ! parser.HasPendingPreparation(), "capacity candidates remain cancellable before rejection");

    std::wstring emails(0u, L' '), invalidHosts(0u, L' '), links(0u, L' ');
    for (std::size_t index = 0u; index < 10000u; ++index)
        emails.append(L"a@");
    emails.append(L"a.com");
    for (std::size_t index = 0u; index < 2500u; ++index)
        invalidHosts.append(L"_www.bad_.invalid");
    for (std::size_t index = 0u; index < 2000u; ++index)
        links.append(L"www.example.com ");
    const std::wstring autolinks[]    = {emails, invalidHosts, links};
    const std::size_t expectedLinks[] = {1u, 0u, 2000u};
    for (std::size_t index = 0u; index < std::size(autolinks); ++index)
    {
        parser.Begin(autolinks[index], 10u + index, MarkdownProfile::Gfm029);
        const auto before = parser.Published();
        for (std::size_t turn = 0u; turn < 64u; ++turn)
            check(parser.Advance(1u).workUnits == 1u && parser.Published() == before,
                  "long autolink candidates remain cooperative and keep their prior immutable publication");
        const auto syntax     = Complete(parser, check);
        const auto statistics = syntax->InlineStatistics();
        check(statistics.autolinkInspectionUnits <= autolinks[index].size() * 8u && statistics.autolinkDecodedUnits <= autolinks[index].size(),
              "host/email indexing and final suffix decoding grow linearly for hostile candidates and many accepted links");
        check(static_cast<std::size_t>(std::ranges::count(syntax->Inlines(), MarkdownInlineKind::Link, &MarkdownInline::kind)) == expectedLinks[index],
              "bounded autolink preparation preserves the independently specified rejected-host and final-email results");
    }
    std::wstring angles(L"x ");
    for (std::size_t index = 0u; index < 16000u; ++index)
        angles.append(L"<a ");
    angles.append(L"<https://example.invalid>");
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        parser.Begin(angles, 20u, profile);
        const auto syntax = Complete(parser, check);
        check(syntax->InlineStatistics().autolinkInspectionUnits >= 32000u && syntax->InlineStatistics().autolinkInspectionUnits <= angles.size() * 8u,
              "rejected angle autolinks inspect only their own invalid prefix");
        check(std::ranges::count(syntax->Inlines(), MarkdownInlineKind::Link, &MarkdownInline::kind) == 1,
              "bounded angle search still recognizes a following valid autolink");
    }
}
template <typename Check> inline void DisplayBlockInterruption(Check&& check)
{
    struct Case
    {
        std::wstring_view prefix;
        MarkdownBlockKind first;
    };
    constexpr Case cases[]{
        {L"> a\n", MarkdownBlockKind::BlockQuote}, {L"- a\n", MarkdownBlockKind::List}, {L"| a |\n| - |\n| 1 |\n", MarkdownBlockKind::Table}};
    for (const auto& item : cases)
        for (const auto opener : {std::wstring_view(L"$$"), std::wstring_view(L"\\[")})
        {
            const auto closer = opener == L"$$" ? L"$$" : L"\\]";
            const auto source = std::wstring(item.prefix).append(opener).append(L"\nx\n").append(closer).append(L"\n");
            MarkdownParser parser;
            parser.Begin(source, 1u, MarkdownProfile::RichMarkdown);
            const auto syntax = Complete(parser, check);
            const auto& nodes = syntax->Nodes();
            const auto first  = nodes[0u].firstChild;
            const auto second = first == MarkdownNoNode ? MarkdownNoNode : nodes[first].nextSibling;
            check(first != MarkdownNoNode && nodes[first].block.kind == item.first && second != MarkdownNoNode &&
                      nodes[second].block.kind == MarkdownBlockKind::FencedCode && nodes[second].block.closed &&
                      nodes[second].block.fenceMarker == (opener == L"$$" ? L'$' : L'[') && nodes[second].nextSibling == MarkdownNoNode,
                  "display math independently interrupts unmatched quote/list continuation and a table body");
            check(syntax->Source() == source, "display interruption preserves exact source");
            if (item.first == MarkdownBlockKind::Table)
                check(std::ranges::count(nodes, MarkdownBlockKind::TableRow, [](const auto& node) { return node.block.kind; }) == 2,
                      "only the header and original body row remain in the interrupted table");
        }
}
template <typename Check> inline void InlineScanBudgets(Check&& check)
{
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
        for (const auto opener : {std::wstring_view(L"<?"), std::wstring_view(L"<!--"), std::wstring_view(L"<![CDATA["), std::wstring_view(L"<!A ")})
        {
            std::size_t previous{};
            for (const std::size_t repeats : {2048u, 4096u})
            {
                // A paragraph prefix keeps these inline. At column zero some
                // openers instead become one opaque block and miss the defect.
                std::wstring source(L"prefix ");
                for (std::size_t index = 0u; index < repeats; ++index)
                    source.append(opener);
                MarkdownParser parser;
                Detail::MarkdownWorkCounts work;
                const Detail::MarkdownWorkProbe probe(work);
                parser.Begin(source, 1u, profile);
                const auto syntax = Complete(parser, check);
                const auto total  = work.htmlIndexUnits + work.htmlSearchedUnits;
                check(total <= source.size() * 8u && (! previous || total <= previous * 5u / 2u),
                      "unterminated HTML searched spans grow linearly when hostile inline input doubles");
                check(syntax->Source() == source && std::ranges::count(syntax->Inlines(), MarkdownInlineKind::Html, &MarkdownInline::kind) == 0,
                      "unterminated inline HTML remains complete literal source in every profile");
                previous = total;
            }
        }
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        std::size_t previous{};
        for (const std::size_t repeats : {4000u, 8000u})
        {
            std::wstring source;
            for (std::size_t index = 0u; index < repeats; ++index)
                source.append(L"[x](");
            MarkdownParser parser;
            Detail::MarkdownWorkCounts work;
            const Detail::MarkdownWorkProbe probe(work);
            parser.Begin(source, 1u, profile);
            const auto syntax = Complete(parser, check);
            check(work.linkDestinationUnits <= source.size() * 140u && (! previous || work.linkDestinationUnits <= previous * 5u / 2u),
                  "rejected link destinations have bounded nesting work and linear doubling");
            check(syntax->Source() == source && std::ranges::count(syntax->Inlines(), MarkdownInlineKind::Link, &MarkdownInline::kind) == 0,
                  "unbalanced destinations never manufacture a link or lose source");
            previous = work.linkDestinationUnits;
        }
        for (const std::size_t depth : {2u, 32u, 33u})
        {
            const auto source = std::wstring(L"[link](foo").append(depth, L'(').append(L"bar").append(depth, L')').append(L")");
            MarkdownParser parser;
            parser.Begin(source, 1u, profile);
            const auto syntax = Complete(parser, check);
            check(std::ranges::count(syntax->Inlines(), MarkdownInlineKind::Link, &MarkdownInline::kind) == (depth <= 32u ? 1 : 0),
                  "balanced destinations allow thirty-two nested parentheses and retain deeper spelling literally");
        }
    }
}
template <typename Check> inline void TableReferenceBudgets(Check&& check)
{
    for (const auto profile : {MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        std::size_t previous{};
        for (const std::size_t lines : {4000u, 8000u})
        {
            std::wstring source(L"[x\n");
            for (std::size_t index = 0u; index < lines; ++index)
                source.append(index % 2u ? L"-|\n" : L"-|-\n");
            MarkdownParser parser;
            Detail::MarkdownWorkCounts work;
            const Detail::MarkdownWorkProbe probe(work);
            parser.Begin(source, 1u, profile);
            const auto syntax = Complete(parser, check);
            check(work.referenceFlattenUnits <= source.size() * 2u && (! previous || work.referenceFlattenUnits <= previous * 5u / 2u),
                  "incompatible table delimiters do not repeatedly flatten a reference-like paragraph");
            const auto& nodes = syntax->Nodes();
            const auto first  = nodes[0u].firstChild;
            check(first != MarkdownNoNode && nodes[first].block.kind == MarkdownBlockKind::Paragraph && nodes[first].nextSibling == MarkdownNoNode &&
                      syntax->Source() == source && std::ranges::count(nodes, MarkdownBlockKind::Table, [](const auto& node) { return node.block.kind; }) == 0,
                  "alternating incompatible cell counts remain one complete paragraph without a table");
            previous = work.referenceFlattenUnits;
        }
    }
}
template <typename Editor, typename Host, typename Prepare, typename Check>
inline void EditorFallback(Editor& editor, Host& host, Prepare&& prepare, Check&& check)
{
    using RenderingStatus = decltype(editor.GetMarkdownRenderingStatus());
    const auto source     = Table(100u, 400u);
    editor.SetText(source);
    editor.SetSelection({1u, 2u});
    const auto revision = editor.Revision();
    prepare();
    check(editor.GetMarkdownRenderingStatus() == RenderingStatus::SourceFallback && ! editor.GetMarkdownSyntax() &&
              ! editor.GetAcknowledgedSemanticDocument(host) && editor.GetText() == source && editor.Revision() == revision &&
              editor.GetSelection() == Selection{1u, 2u} && ! editor.CanUndo(),
          "parser capacity publishes exact usable source fallback without a partial tree, text edit, selection change or history item");
    check(editor.TryGetTextInputCaretRect(host, 1u).has_value(), "capacity fallback has current native source geometry");
    const auto advances = editor.DebugGetState().markdownPreparationAdvances;
    for (unsigned turn = 0u; turn < 16u; ++turn)
        prepare();
    using Flow      = decltype(editor.GetFlowDirection());
    const auto flow = editor.GetFlowDirection();
    editor.SetFlowDirection(Flow::RightToLeft);
    prepare();
    editor.SetFlowDirection(flow);
    prepare();
    check(editor.DebugGetState().markdownPreparationAdvances == advances,
          "idle and typography/flow preparation cannot repeatedly parse a rejected exact source request");
    editor.SetSelection({0u, source.size()});
    check(editor.ReplaceSelection(L"# small"), "an overflow document remains source-editable");
    prepare();
    check(editor.GetMarkdownRenderingStatus() == RenderingStatus::Semantic && editor.GetMarkdownSyntax() && editor.CanUndo(),
          "a smaller source edit clears capacity rejection and restores rich preparation");
    check(editor.Undo() && editor.GetText() == source, "undo restores exact overflow source");
    prepare();
    check(editor.GetMarkdownRenderingStatus() == RenderingStatus::SourceFallback, "undo's new revision safely re-evaluates capacity");
    const auto retryAdvances = editor.DebugGetState().markdownPreparationAdvances;
    prepare();
    check(editor.DebugGetState().markdownPreparationAdvances == retryAdvances, "restored overflow rejection coalesces after its single new attempt");
    editor.SetMarkdownAnalysisProfile(MarkdownProfile::CommonMark0312);
    prepare();
    check(editor.GetMarkdownRenderingStatus() == RenderingStatus::Semantic && editor.GetMarkdownSyntax() && editor.GetText() == source,
          "a profile change retries the same source under its own non-table grammar");
    editor.SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    prepare();
    check(editor.GetMarkdownRenderingStatus() == RenderingStatus::SourceFallback, "switching back cannot reuse another profile's admitted tree");
    check(editor.Redo() && editor.GetText() == L"# small", "capacity/profile preparation preserves redo history");
    prepare();
    check(editor.GetMarkdownRenderingStatus() == RenderingStatus::Semantic, "redo recovers current semantic preparation");
}
} // namespace MarkdownCapacityTests
