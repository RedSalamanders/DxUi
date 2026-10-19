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
    Check(editor->OnKeyDown(host, VK_RIGHT, 0u) && editor->GetSelection().caret == 2u,
          "Right visits an empty physical source line before the next content anchor");
    Check(editor->OnKeyDown(host, VK_RIGHT, 0u) && editor->GetSelection().caret == 3u, "Right leaves the empty source line for the next paragraph");
    Check(editor->OnKeyDown(host, VK_LEFT, 0u) && editor->GetSelection().caret == 2u, "Left revisits the empty source line");
    Check(editor->OnKeyDown(host, VK_LEFT, 0u) && editor->GetSelection().caret == 1u, "Left crosses the separator to the previous paragraph end");
    editor->SetSelection({4u, 4u});
    Check(editor->OnKeyDown(host, VK_LEFT, 0u) && editor->GetSelection().caret == 3u,
          "Left reaches the next paragraph's own beginning before crossing its separator");
    editor->SetMarkdownAnalysisProfile(std::nullopt);
    Check(! editor->IsMarkdownRenderingEnabled() && ! editor->GetSemanticDocument() && document->Map().Source() == original,
          "clearing the dialect disables semantic rendering and held results remain immutable");
    prepare();
}
// Expose the protected text-service import seam for deterministic composition
// transport tests. Physical IME/foreground qualification remains separate.
struct SourceCaretEditorFixture final : RichTextEditor
{
    using RichTextEditor::ExportTextInputState;
    using RichTextEditor::ImportTextInputState;
    using RichTextEditor::RichTextEditor;
};
template <typename Prepare> void SourceCaretSites(ControlHost& host, Prepare&& prepare)
{
    const std::wstring source = L"\r\n>\r\n> >\n- \r\n\r\n";
    auto root                 = std::make_unique<SourceCaretEditorFixture>(source);
    auto* editor              = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    host.SetRoot(std::move(root));
    prepare();
    const auto revision = editor->Revision();
    const auto syntax   = editor->GetMarkdownSyntax();
    Check(syntax && syntax->EmptyLines().size() == 6u, "retained source caret fixture includes empty, nested-container and trailing lines");
    float previousY = -1.0f;
    for (const auto& line : syntax->EmptyLines())
    {
        editor->SetSelection({line.insertion, line.insertion});
        const auto caret = editor->TryGetTextInputCaretRect(host, line.insertion);
        Check(caret && caret->top > previousY && caret->bottom > caret->top, "native/embedded source caret sites have distinct text-service bounds");
        const auto hit = editor->TryHitTestTextInputPoint(host, D2D1::Point2F(caret->left, (caret->top + caret->bottom) * .5f));
        Check(hit && *hit == line.insertion, "native/embedded empty caret bounds and text-service hit testing agree in raw ACPs");
        previousY = caret->top;
    }
    Check(editor->GetText() == source && editor->Revision() == revision && ! editor->CanUndo(),
          "placing every source caret preserves text, revision and history");
    Check(editor->OnKeyDown(host, VK_HOME, MK_CONTROL) && editor->GetSelection().caret == 0u, "document Home preserves exact raw zero");
    Check(editor->OnKeyDown(host, VK_DOWN, 0u) && editor->GetSelection().caret == syntax->EmptyLines()[1u].insertion, "Down visits the next empty quote line");
    Check(editor->OnKeyDown(host, VK_UP, 0u) && editor->GetSelection().caret == 0u, "Up returns to the leading empty line");
    editor->SetSelection({source.size(), source.size()});
    Check(editor->OnChar(host, L'X', 0u) && editor->GetText() == source + L"X", "typing at the trailing caret inserts at the exact source EOF");
    prepare();
    Check(editor->Undo() && editor->GetText() == source && ! editor->CanUndo(), "trailing caret insertion is one exact-source undo step");
    prepare();
    Check(editor->Redo() && editor->GetText() == source + L"X", "trailing caret insertion redoes without invented blank source");
    prepare();

    const std::wstring table = L"| a | b | c |\n| - | - | - |\n| value |\n";
    editor->SetText(table);
    prepare();
    const auto value    = table.find(L"value");
    const auto bounds   = editor->TryGetTextInputRangeRects(host, value, value + 5u);
    const auto viewport = editor->TryGetTextInputViewportRect();
    Check(bounds && ! bounds->empty() && viewport, "retained padded-cell fixture has current native geometry");
    const float width = viewport->right - viewport->left;
    const auto point  = D2D1::Point2F(viewport->left + width * .5f, bounds->front().top + 5.0f);
    Check(editor->OnMouseDown(host, point, false, 0u), "native/embedded pointer chooses a padded cell");
    static_cast<void>(editor->OnMouseUp(host, point, false, 0u));
    const auto anchor = table.rfind(L'\n');
    Check(editor->GetSelection() == Selection{anchor, anchor}, "padded cell exposes the raw row-end selection rather than a virtual ACP");
    const auto first = editor->TryGetTextInputCaretRect(host, anchor);
    Check(first && editor->OnKeyDown(host, VK_RIGHT, 0u) && editor->GetSelection() == Selection{anchor, anchor},
          "Right changes padded-cell affinity without changing raw selection");
    const auto second = editor->TryGetTextInputCaretRect(host, anchor);
    Check(second && second->left > first->left + width * .2f, "equal-ACP padded cells have visibly different retained carets");
    Check(editor->GetText() == table && ! editor->CanUndo(), "clicking and navigating padded cells does not materialize source or history");
    Check(editor->OnKeyDown(host, VK_HOME, 0u) && editor->OnKeyDown(host, VK_END, 0u), "padded-cell Home and End are handled");
    const auto stillSecond = editor->TryGetTextInputCaretRect(host, anchor);
    Check(stillSecond && std::abs(stillSecond->left - second->left) < .1f, "padded-cell Home/End keep visual affinity");
    TextInputState input;
    Check(editor->ExportTextInputState(input) && input.text == table && input.caretIndex == anchor,
          "source text-service export contains exact text and the real padded-cell ACP without requiring OS focus");
    Check(editor->ImportTextInputState(host, input, false), "selection-only text-service synchronization accepts the active source caret");
    const auto synchronized = editor->TryGetTextInputCaretRect(host, anchor);
    Check(synchronized && std::abs(synchronized->left - second->left) < .1f, "unchanged TSF synchronization preserves padded-cell affinity");
    editor->SetReadOnly(true);
    static_cast<void>(editor->OnChar(host, L'Z', 0u));
    Check(editor->GetText() == table && ! editor->CanUndo(), "read-only padded caret cannot insert text");
    editor->SetReadOnly(false);
    editor->SetVisible(false);
    Check(! editor->TryGetTextInputCaretRect(host, anchor), "hidden source caret geometry is unavailable");
    editor->SetVisible(true);
    prepare();
    const auto restored = editor->TryGetTextInputCaretRect(host, anchor);
    Check(restored && std::abs(restored->left - second->left) < .1f, "geometry retirement and recreation preserve same-source cell affinity");
    editor->SetEnabled(false);
    static_cast<void>(editor->OnChar(host, L'Z', 0u));
    Check(! editor->OnMouseDown(host, point, false, 0u) && editor->GetText() == table && ! editor->CanUndo(),
          "disabled padded-cell sites accept neither pointer placement nor typing");
    editor->SetEnabled(true);
    static_cast<void>(editor->OnKeyDown(host, VK_BACK, 0u));
    Check(editor->GetText() == table && ! editor->CanUndo(), "a generated empty-cell caret never deletes an unrelated owned cell token");
    editor->SetViewMode(ViewMode::Source);
    prepare();
    Check(editor->TryGetTextInputCaretRect(host, anchor).has_value() && editor->GetText() == table && ! editor->CanUndo(),
          "source view preserves exact text and provides ordinary raw source caret geometry");
    editor->SetViewMode(ViewMode::Rich);
    editor->SetText(L">\r\n\r\n");
    Check(! editor->TryGetTextInputCaretRect(host, 1u), "source replacement immediately rejects stale cell affinity and geometry");
    prepare();
    const auto entry = editor->GetText();
    editor->SetSelection({1u, 1u});
    TextInputState originalInput;
    Check(editor->ExportTextInputState(originalInput), "export empty quote source state");
    auto preview = originalInput;
    preview.text.insert(1u, L"\u6771\u4eac");
    preview.caretIndex = 3u;
    Check(editor->ImportTextInputState(host, preview, false), "text-service preview inserts Unicode at an exact empty quote ACP");
    prepare();
    Check(editor->GetText() == L">\u6771\u4eac\r\n\r\n" && ! editor->CanUndo(),
          "transient source preview preserves container spelling and CRLF without history");
    Check(editor->ImportTextInputState(host, originalInput, false), "text-service cancellation restores empty source site");
    prepare();
    Check(editor->GetText() == entry && ! editor->CanUndo() && editor->TryGetTextInputCaretRect(host, 1u),
          "cancel restores the raw empty caret without an undo entry");
    Check(editor->ImportTextInputState(host, preview, true), "text-service commit applies exact Unicode source once");
    prepare();
    Check(editor->Undo() && editor->GetText() == entry && ! editor->CanUndo(), "one undo restores the empty container after text-service commit");
    prepare();
    editor->SetText(L"\r\n\r\n");
    prepare();
    editor->SetSelection({2u, 2u});
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText() == L"\r\n", "empty physical-line Backspace deletes exactly one whole raw CRLF");
    prepare();
    Check(editor->Undo() && editor->GetText() == L"\r\n\r\n", "empty-line Backspace is one reversible raw source edit");
    prepare();
    editor->SetText(L"- [ ] one");
    prepare();
    editor->SetSelection({0u, 0u});
    Check(editor->OnKeyDown(host, VK_LEFT, 0u) && editor->GetSelection().caret == 0u && editor->OnKeyDown(host, VK_LEFT, MK_CONTROL) &&
              editor->GetSelection().caret == 0u,
          "Left and word-Left at raw document zero never jump forward through a generated marker anchor");
    editor->SetText(L"a\r\n\r\nb\r\n");
    prepare();
    const auto sourceLines = editor->GetSourceVisualLineRanges(host);
    const std::array<std::pair<std::size_t, std::size_t>, 4u> expectedLines{{{0u, 3u}, {3u, 5u}, {5u, 8u}, {8u, 8u}}};
    Check(sourceLines && std::ranges::equal(*sourceLines, expectedLines), "native/embedded line-unit publication has no phantom EOL-only source line");
    for (const auto sourceText : {L"&amp;", L"&fjlig;", L"![alt](local)"})
    {
        editor->SetText(sourceText);
        prepare();
        editor->SetSelection({1u, 1u});
        Check(editor->OnKeyDown(host, VK_LEFT, 0u) && editor->GetSelection().caret == 0u,
              "native/embedded Left from an interior atomic source ACP reaches its leading boundary");
        editor->SetSelection({1u, 1u});
        Check(editor->OnKeyDown(host, VK_RIGHT, 0u) && editor->GetSelection().caret == std::wstring_view(sourceText).size() &&
                  editor->GetText() == sourceText && ! editor->CanUndo(),
              "native/embedded Right reaches the trailing atomic boundary without source/history mutation");
    }
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
    editor->SetText(std::wstring(SemanticDocument::MaximumParagraphs + 1u, L'\n'));
    prepare();
    Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::SourceFallback && editor->DebugGetState().layoutReady,
          "native empty-caret scaffolding shares the bounded paragraph admission ceiling and falls back to exact source");
    const auto nativeAdvances = editor->DebugGetState().preparationAdvances;
    prepare();
    Check(editor->DebugGetState().preparationAdvances == nativeAdvances && ! editor->CanUndo(),
          "native caret admission rejection is not retried by idle owner turns");
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
