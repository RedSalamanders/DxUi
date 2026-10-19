#pragma once
#include "Scope.Tests.RichSemantic.h"
#include <DxUi/DxUi.h>

namespace RichSemanticEditorTests
{
using namespace DxUi;
using namespace DxUi::RichText;
using RichSemanticTests::Check;
template <typename Prepare> void Rendering(ControlHost& host, Prepare&& prepare)
{
    const std::wstring original = L"# **Heading**\r\n\r\n| left | right |\n| :--- | ---: |\n| a | b |\n\n> quoted &amp; **bold**\n\n![alt](local)\n";
    auto root                   = std::make_unique<RichTextEditor>(original);
    auto* editor                = root.get();
    RichSemanticTests::Reject<std::invalid_argument>([&] { editor->SetMarkdownRenderingEnabled(true); }, "rendering requires an explicit dialect");
    Check(! editor->IsMarkdownRenderingEnabled(), "rejected rendering opt-in has no side effects");
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    host.SetRoot(std::move(root));
    prepare();
    Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Disabled && ! editor->GetSemanticDocument(), "analysis alone keeps the v0 renderer");
    editor->SetSelection({7u, 4u});
    editor->SetMarkdownRenderingEnabled(true);
    Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Pending && ! editor->DebugGetState().layoutReady,
          "semantic opt-in immediately retires incompatible native geometry");
    prepare();
    const auto document = editor->GetSemanticDocument();
    Check(document && document->Syntax() == editor->GetMarkdownSyntax() && document->Map().Source() == original &&
              editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && editor->GetSelection() == Selection{7u, 4u} && ! editor->CanUndo(),
          "native and embedded owners publish one current semantic graph without editing source/history");
    const auto hidden = editor->TryGetTextInputRangeRects(host, 0u, 2u);
    Check(hidden && hidden->empty(), "hidden heading syntax has no text-service glyph rectangle");
    const auto left = original.find(L"left"), right = original.find(L"right");
    const auto leftBounds  = editor->TryGetTextInputRangeRects(host, left, left + 4u);
    const auto rightBounds = editor->TryGetTextInputRangeRects(host, right, right + 5u);
    Check(leftBounds && rightBounds && ! leftBounds->empty() && ! rightBounds->empty() && leftBounds->front().left < rightBounds->front().left &&
              std::abs(leftBounds->front().top - rightBounds->front().top) < .1f,
          "retained native/embedded table cells share a row and retain distinct column geometry");
    const auto lines = editor->GetSourceVisualLineRanges(host);
    Check(lines && ! lines->empty() && lines->front().first == 0u && lines->back().second == original.size(),
          "semantic source line units cover exact raw ACPs");
    for (size_t i = 1u; i < lines->size(); ++i)
        Check((*lines)[i - 1u].second == (*lines)[i].first, "semantic source line units never reverse, overlap or truncate after a table cell");
    const auto image = original.find(L"local");
    Check(editor->TryGetTextInputRangeRects(host, image, image + 5u)->size() > 0u, "atomic image metadata reveals its semantic alternative");
    Check(editor->ScrollSourceRangeIntoView(host, image, image + 5u, true), "semantic object source range scrolls without taking focus");
    prepare();
    editor->SetFlowDirection(FlowDirection::RightToLeft);
    prepare();
    const auto rtlLeft  = editor->TryGetTextInputRangeRects(host, left, left + 4u);
    const auto rtlRight = editor->TryGetTextInputRangeRects(host, right, right + 5u);
    Check(rtlLeft && rtlRight && rtlLeft->front().left > rtlRight->front().left && editor->GetSemanticDocument() == document,
          "flow repositions native table geometry while reusing immutable CPU semantics");
    editor->SetVisible(false);
    prepare();
    Check(! editor->DebugGetState().layoutReady && ! editor->DebugGetState().preparationPending,
          "hidden semantic editor retires every SDK graph and pending cursor");
    editor->SetVisible(true);
    prepare();
    const auto semanticGeneration = editor->DebugGetState().layoutGeneration;
    editor->SetViewMode(ViewMode::Source);
    prepare();
    Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Disabled && ! editor->TryGetTextInputRangeRects(host, 0u, 2u)->empty(),
          "source mode exposes exact Markdown characters without changing the opt-in");
    Check(editor->DebugGetState().layoutGeneration > semanticGeneration, "switching layout engines cannot reuse a retired publication generation");
    editor->SetViewMode(ViewMode::Rich);
    editor->SetFlowDirection(FlowDirection::LeftToRight);
    editor->SetText(L"&fjlig; A\U0001f600\u0301\n");
    prepare();
    editor->SetSelection({0u, 0u});
    Check(editor->OnKeyDown(host, VK_RIGHT, 0u) && editor->GetSelection().caret == 7u, "semantic keyboard navigation crosses a complete decoded entity");
    editor->SetSelection({3u, 3u});
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText() == L" A\U0001f600\u0301\n", "raw interior entity caret deletes the complete visual token");
    Check(editor->Undo() && editor->GetText() == L"&fjlig; A\U0001f600\u0301\n", "atomic semantic deletion is one undo transaction");
    prepare();
    Check(editor->OnKeyDown(host, VK_END, MK_CONTROL) && editor->GetSelection().caret == editor->GetText().size(),
          "semantic document end includes trailing raw trivia");
    editor->SetText(L"a\n\nb");
    prepare();
    editor->SetSelection({1u, 1u});
    Check(editor->OnKeyDown(host, VK_RIGHT, 0u) && editor->GetSelection().caret == 3u,
          "Right crosses a generated paragraph separator to the next content anchor");
    Check(editor->OnKeyDown(host, VK_LEFT, 0u) && editor->GetSelection().caret == 1u, "Left crosses the separator to the previous paragraph end");
    editor->SetSelection({4u, 4u});
    Check(editor->OnKeyDown(host, VK_LEFT, 0u) && editor->GetSelection().caret == 3u,
          "Left reaches the next paragraph's own beginning before crossing its separator");
    editor->SetMarkdownAnalysisProfile(std::nullopt);
    Check(! editor->IsMarkdownRenderingEnabled() && ! editor->GetSemanticDocument() && document->Map().Source() == original,
          "clearing the dialect disables semantic rendering and held results remain immutable");
    prepare();
}
template <typename Prepare> void RenderingAdmissionFallback(ControlHost& host, Prepare&& prepare)
{
    std::wstring source;
    for (size_t i = 0u; i <= SemanticDocument::MaximumParagraphs; ++i)
        source += L"x\n\n";
    auto root    = std::make_unique<RichTextEditor>(source);
    auto* editor = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    host.SetRoot(std::move(root));
    prepare();
    Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::SourceFallback && editor->DebugGetState().layoutReady &&
              ! editor->GetSemanticDocument() && editor->GetText() == source && ! editor->CanUndo() && editor->GetViewMode() == ViewMode::Rich,
          "semantic admission overflow keeps exact source editing and requested mode without a retry loop");
    const auto advances = editor->DebugGetState().semanticPreparationAdvances;
    prepare();
    Check(editor->DebugGetState().semanticPreparationAdvances == advances, "an unchanged rejected semantic revision never retries during idle");
    editor->SetText(L"**admitted**\n");
    prepare();
    Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && editor->GetSemanticDocument()->Map().Display() == L"admitted\n",
          "a new source revision retries and leaves the explicit source fallback");
}
template <typename Prepare> void Transactions(ControlHost& host, Prepare&& prepare)
{
    const std::wstring original = L"- [ ] one\r\n- [x] two\n";
    const std::wstring changed  = L"- [x] one\r\n- [ ] two\n";
    auto root                   = std::make_unique<RichTextEditor>(original);
    auto* editor                = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    host.SetRoot(std::move(root));
    prepare();
    const auto oldSyntax = editor->GetMarkdownSyntax();
    const auto snapshot  = editor->Snapshot();
    const auto revision  = editor->Revision();
    Check(oldSyntax && oldSyntax->Source() == original && editor->DebugGetState().layoutReady, "retained transaction starts from acknowledged syntax/geometry");
    size_t notifications = 0u;
    editor->SetOnTextChanged([&] { ++notifications; });
    editor->SetSelection({9u, 6u});
    const std::array edits{SourceEdit{{3u, 4u}, L"x"}, SourceEdit{{14u, 15u}, L" "}};
    Check(editor->ApplySourceEdits(snapshot, revision, edits, {9u, 6u}) && editor->GetText() == changed && notifications == 1u,
          "native/embedded source plan publishes once with exact mixed EOLs");
    Check(! editor->GetMarkdownSyntax() && ! editor->DebugGetState().layoutReady && oldSyntax->Source() == original,
          "atomic transaction rejects stale queries and preserves held syntax");
    prepare();
    Check(editor->GetMarkdownSyntax() && editor->GetMarkdownSyntax()->Source() == changed && editor->DebugGetState().layoutReady,
          "host prepares complete new source/analysis/geometry after source plan");
    Check(editor->Undo() && editor->GetText() == original && editor->GetSelection() == Selection{9u, 6u} && ! editor->CanUndo() && notifications == 2u,
          "one retained undo restores the complete multi-edit transaction and direction");
    prepare();
    Check(editor->Redo() && editor->GetText() == changed && notifications == 3u, "one retained redo reapplies the plan");
    prepare();
    const auto current         = editor->Snapshot();
    const auto currentRevision = editor->Revision();
    const auto currentSyntax   = editor->GetMarkdownSyntax();
    const auto generation      = editor->DebugGetState().layoutGeneration;
    Check(! editor->ApplySourceEdits(current, currentRevision, edits, {9u, 0u}) && notifications == 3u && editor->GetSelection() == Selection{9u, 0u} &&
              editor->GetMarkdownSyntax() == currentSyntax,
          "same spelling updates selection silently without invalidating analysis");
    prepare();
    Check(editor->DebugGetState().layoutGeneration == generation && editor->Revision() == currentRevision, "same-content plan reuses geometry and revision");
    RichSemanticTests::Reject<std::invalid_argument>([&] { editor->ApplySourceEdits(snapshot, revision, edits, {}); }, "retained stale plan rejected");
    Check(editor->GetText() == changed && notifications == 3u && editor->GetSelection() == Selection{9u, 0u},
          "stale plan has no notification or selection effect");
    editor->SetReadOnly(true);
    Check(! editor->ApplySourceEdits(snapshot, revision, edits, {}) && notifications == 3u, "read-only plan rejects before source mutation");
    editor->SetReadOnly(false);
    editor->SetEnabled(false);
    Check(! editor->ApplySourceEdits(snapshot, revision, edits, {}) && notifications == 3u, "disabled plan rejects before source mutation");
    editor->SetEnabled(true);
    editor->SetOnTextChanged([] { throw std::runtime_error("planned callback failure"); });
    const std::array restore{SourceEdit{{3u, 4u}, L" "}, SourceEdit{{14u, 15u}, L"x"}};
    Check(editor->ApplySourceEdits(current, currentRevision, restore, {6u, 9u}) && editor->GetText() == original,
          "notification exception is contained after an atomic committed plan");
    prepare();
    notifications        = 0u;
    bool sawCompletePlan = false;
    editor->SetOnTextChanged([&]
    {
        ++notifications;
        if (notifications == 1u)
        {
            sawCompletePlan = editor->GetText() == changed;
            Check(editor->Undo(), "notification can undo one complete source plan reentrantly");
        }
    });
    Check(editor->ApplySourceEdits(editor->Snapshot(), editor->Revision(), edits, {9u, 6u}) && sawCompletePlan && editor->GetText() == original &&
              notifications == 2u,
          "reentrant notification never observes a partly applied plan");
    prepare();
    editor->SetOnTextChanged([&] { host.SetRoot(std::make_unique<Label>(L"replacement")); });
    Check(editor->ApplySourceEdits(editor->Snapshot(), editor->Revision(), edits, {}) && dynamic_cast<Label*>(host.GetRoot()),
          "source-plan callback may destroy its editor without a stale access");
    prepare();
}
} // namespace RichSemanticEditorTests
