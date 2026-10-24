#pragma once
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
    const std::wstring rejected[] = {Table(100u, 400u), Table(1u, 10922u), global, dense, html};
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
    parser.Begin(pending, 5u, MarkdownProfile::Gfm029);
    // The two header lines contain exactly 6,002 raw units. The following
    // 128 one-event turns are inside generation of the 1,000 header cells.
    for (std::size_t step = 0u; step < 6130u; ++step)
        check(parser.Advance(1u).workUnits == 1u && parser.Published()->Source() == L"retry",
              "table-cell preparation yields without exposing its incomplete row");
    parser.Cancel();
    check(parser.Published()->Source() == L"retry" && ! parser.HasPendingPreparation(), "capacity candidates remain cancellable before rejection");
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
