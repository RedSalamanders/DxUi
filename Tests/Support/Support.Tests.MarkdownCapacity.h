#pragma once
#include "Controls/DxUi.MarkdownBlocks.h"
#include <DxUi/MarkdownSyntax.h>
#include <stdexcept>

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
        bool capacity{};
        try
        {
            Complete(parser, check);
        }
        catch (const std::length_error&)
        {
            capacity = true;
        }
        check(capacity && parser.Published() == previous && ! parser.HasPendingPreparation() && previous->Source() == L"old" && old->Source() == L"old",
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
