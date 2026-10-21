#pragma once
#include "Scope.Tests.RichSemantic.h"
#include <DxUi/DxUi.h>
#include <cmath>

namespace RichSemanticEditorTests
{
using namespace DxUi;
using namespace DxUi::RichText;
using RichSemanticTests::Check;
struct RetiringTabEditor final : RichTextEditor
{
    explicit RetiringTabEditor(Button*& successor) : RichTextEditor(L"retiring"), successor_(&successor)
    {
    }
    bool OnKeyDown(ControlHost& host, UINT key, UINT) noexcept override
    {
        if (key != VK_TAB)
            return false;
        try
        {
            auto root   = std::make_unique<Panel>();
            auto* first = root->AddChild<Button>(L"first");
            first->SetBounds(D2D1::RectF(8, 8, 160, 40));
            root->AddChild<Button>(L"second")->SetBounds(D2D1::RectF(8, 48, 160, 80));
            auto* output = successor_;
            *output      = first;
            host.SetRoot(std::move(root)); // Retires this object. Only locals follow.
            host.SetFocusControl(first, false);
            return false;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

private:
    Button** successor_;
};
template <typename Prepare, typename Key, typename Character> void StructuralHostRouting(ControlHost& host, Prepare&& prepare, Key&& key, Character&& character)
{
    auto panel   = std::make_unique<Panel>();
    auto* editor = panel->AddChild<RichTextEditor>(L"- a\n- b\n");
    editor->SetBounds(D2D1::RectF(8, 8, 464, 190));
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    auto* next = panel->AddChild<Button>(L"Next field");
    next->SetBounds(D2D1::RectF(8, 200, 180, 232));
    host.SetRoot(std::move(panel));
    prepare();
    host.SetFocusControl(editor, false);
    Check(key(VK_HOME, 0u) && host.GetFocusControl() == editor, "actual host key transport reaches the focused editor before Tab testing");
    editor->SetSelection({6u, 6u});
    Check(key(VK_TAB, 0u) && editor->GetText() == L"- a\n  - b\n" && host.GetFocusControl() == editor,
          "actual native/embedded Tab dispatch offers list indentation before focus traversal");
    prepare();
    Check(key(VK_TAB, MK_SHIFT) && editor->GetText() == L"- a\n- b\n" && host.GetFocusControl() == editor, "actual Shift+Tab dispatch outdents in place");
    editor->SetText(L"- item\n");
    prepare();
    editor->SetSelection({6u, 6u});
    Check(character(L'\r', 0u) && editor->GetText() == L"- item\n- \n", "actual native/embedded character dispatch continues a list");
    editor->SetText(L"a | b | c\n- | - | -\nx\n");
    prepare();
    const auto syntax = editor->GetMarkdownSyntax();
    std::vector<std::size_t> cells(0u);
    for (std::size_t i = 0u; i < syntax->Nodes().size(); ++i)
        if (syntax->Nodes()[i].block.kind == MarkdownBlockKind::TableCell)
            cells.push_back(i);
    editor->SetSelection({editor->GetText().size() - 1u, editor->GetText().size() - 1u});
    MarkdownCommandRequest request;
    request.command    = MarkdownCommand::NextTableCell;
    request.targetCell = cells[3u];
    Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Unchanged, "transport fixture reaches first padded cell");
    Check(key(VK_TAB, 0u) && host.GetFocusControl() == editor && character(L'|', 0u) && editor->GetText() == L"a | b | c\n- | - | -\nx |  | \\|\n",
          "actual host key/character dispatch retains padded-cell affinity and materializes the clicked column");
    editor->SetText(L"word");
    prepare();
    editor->SetSelection({4u, 0u});
    Check(key('B', MK_CONTROL) && editor->GetText() == L"**word**" && editor->GetSelection() == Selection{6u, 2u},
          "actual host Ctrl+B route applies one directional source style command");
    editor->SetText(L"word");
    prepare();
    Check(key(VK_TAB, 0u) && host.GetFocusControl() == next && editor->GetText() == L"word", "ordinary paragraph Tab reaches the next host control");
    Button* successor = nullptr;
    auto retiring     = std::make_unique<RetiringTabEditor>(successor);
    auto* old         = retiring.get();
    host.SetRoot(std::move(retiring));
    prepare();
    host.SetFocusControl(old, false);
    Check(key(VK_TAB, 0u) && successor && host.GetFocusControl() == successor,
          "declined Tab that retires its owner cannot traverse the successor in the same frame");
    prepare();
}
template <typename Prepare> void StructuralCommands(ControlHost& host, Prepare&& prepare)
{
    auto root    = std::make_unique<RichTextEditor>(L"word");
    auto* editor = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    host.SetRoot(std::move(root));
    prepare();
    host.SetFocusControl(editor, false);
    std::size_t notifications = 0u;
    editor->SetOnTextChanged([&] { ++notifications; });
    const auto exact = [&](std::wstring_view source, Selection selection, MarkdownCommandRequest request, std::wstring_view expected)
    {
        editor->SetText(std::wstring(source));
        prepare();
        editor->SetSelection(selection);
        const auto old   = editor->Snapshot();
        const auto count = notifications;
        Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Changed && editor->GetText() == expected && notifications == count + 1u,
              "retained command publishes complete source once in both hosts");
        Check(editor->Undo() && editor->Snapshot().IsSameRevision(old) && editor->GetSelection() == selection && ! editor->CanUndo(),
              "retained command one exact undo");
        Check(editor->Redo() && editor->GetText() == expected && ! editor->CanRedo(), "retained command one redo");
        prepare();
        Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && editor->GetSemanticDocument()->Map().Source() == expected,
              "retained command re-prepares through owner scheduling");
    };
    MarkdownCommandRequest request;
    request.command = MarkdownCommand::SetList;
    exact(L"word", {0u, 4u}, request, L"- word");
    request.command      = MarkdownCommand::SetHeading;
    request.headingLevel = 2u;
    exact(L"word", {2u, 2u}, request, L"## word");
    request.command = MarkdownCommand::ToggleStyle;
    exact(L"word", {4u, 0u}, request, L"**word**");
    request.command     = MarkdownCommand::SetLink;
    request.destination = L"target";
    exact(L"word", {0u, 4u}, request, L"[word](<target>)");
    request.command = MarkdownCommand::ToggleTask;
    exact(L"- [ ] word\n", {8u, 8u}, request, L"- [x] word\n");
    request.command = MarkdownCommand::SetTableCell;
    request.text    = L"new";
    exact(L"a | b\n- | -\nx | y", {16u, 16u}, request, L"a | b\n- | -\nx | new");
    editor->SetText(L"- item\n");
    prepare();
    editor->SetSelection({6u, 6u});
    Check(editor->OnChar(host, L'\r', 0u) && editor->GetText() == L"- item\n- \n", "semantic Enter continues list through ControlHost input");
    Check(editor->Undo(), "keyboard list continuation is undoable");
    prepare();
    editor->SetText(L"word");
    prepare();
    editor->SetSelection({4u, 0u});
    Check(editor->OnKeyDown(host, 'B', MK_CONTROL) && editor->GetText() == L"**word**" && editor->GetSelection() == Selection{6u, 2u},
          "Ctrl+B uses source-exact formatting and backward selection");
    Check(editor->Undo(), "keyboard style undo");
    prepare();
    editor->SetText(L"- a\n- b\n");
    prepare();
    editor->SetSelection({6u, 6u});
    Check(editor->OnKeyDown(host, VK_TAB, 0u) && editor->GetText() == L"- a\n  - b\n", "Tab indents an eligible list before host traversal");
    prepare();
    Check(editor->OnKeyDown(host, VK_TAB, MK_SHIFT) && editor->GetText() == L"- a\n- b\n", "Shift+Tab outdents list");
    editor->SetText(L"word");
    prepare();
    editor->SetSelection({2u, 2u});
    Check(! editor->OnKeyDown(host, VK_TAB, 0u), "ordinary paragraph Tab remains host traversal");
    editor->SetText(L"a | b | c\n- | - | -\nx\n");
    prepare();
    const auto syntax = editor->GetMarkdownSyntax();
    std::vector<std::size_t> cells(0u);
    for (std::size_t i = 0u; i < syntax->Nodes().size(); ++i)
        if (syntax->Nodes()[i].block.kind == MarkdownBlockKind::TableCell)
            cells.push_back(i);
    editor->SetSelection({editor->GetText().size() - 1u, editor->GetText().size() - 1u});
    request.command    = MarkdownCommand::NextTableCell;
    request.targetCell = cells[3u];
    Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Unchanged && ! editor->CanUndo(),
          "table navigation stores visual affinity without source history");
    Check(editor->OnKeyDown(host, VK_TAB, 0u) && ! editor->CanUndo(), "Tab crosses two padded cells sharing one raw ACP");
    Check(editor->OnChar(host, L'|', 0u) && editor->GetText() == L"a | b | c\n- | - | -\nx |  | \\|\n",
          "typing materializes only the focused padded cell and escapes a literal pipe");
    editor->SetText(L"- [ ] task\n");
    prepare();
    const auto bounds = editor->TryGetTextInputRangeRects(host, 2u, 5u);
    Check(bounds && ! bounds->empty(), "task marker has an actual native checkbox hit rectangle");
    const D2D1_POINT_2F point{(bounds->front().left + bounds->front().right) * .5f, (bounds->front().top + bounds->front().bottom) * .5f};
    const auto count = notifications;
    Check(editor->OnMouseDown(host, point, false, 0u) && editor->GetText() == L"- [ ] task\n", "task press stages a plan without mutating source");
    Check(editor->OnMouseUp(host, point, false, 0u) && editor->GetText() == L"- [x] task\n" && notifications == count + 1u,
          "matching task release commits one source transaction");
    editor->Undo();
    prepare();
    Check(editor->OnMouseDown(host, point, false, 0u), "task cancellation press");
    host.ResetInteractionState();
    editor->OnMouseUp(host, point, false, 0u);
    Check(editor->GetText() == L"- [ ] task\n", "capture loss cancels task gesture");
    host.ReleaseMouseCapture();
    host.SetFocusControl(editor, false);
    Check(editor->OnMouseDown(host, point, false, 0u), "task drag cancellation press");
    editor->OnMouseMove(host, {point.x + 300.f, point.y}, 0u);
    editor->OnMouseUp(host, point, false, 0u);
    Check(editor->GetText() == L"- [ ] task\n", "leaving checkbox cancels the source command");
    prepare();
    Check(editor->OnMouseDown(host, point, false, 0u), "task layout retirement press");
    editor->SetFlowDirection(FlowDirection::RightToLeft);
    editor->OnMouseUp(host, point, false, 0u);
    Check(editor->GetText() == L"- [ ] task\n", "native/embedded layout retirement cancels staged task plan");
    prepare();
    const auto rtlBounds = editor->TryGetTextInputRangeRects(host, 2u, 5u);
    Check(rtlBounds && ! rtlBounds->empty(), "RTL task checkbox has native hit geometry");
    const D2D1_POINT_2F rtlPoint{(rtlBounds->front().left + rtlBounds->front().right) * .5f, (rtlBounds->front().top + rtlBounds->front().bottom) * .5f};
    Check(editor->OnMouseDown(host, rtlPoint, false, 0u) && editor->OnMouseUp(host, rtlPoint, false, 0u) && editor->GetText() == L"- [x] task\n",
          "native/embedded RTL task release edits the clicked source marker");
    editor->Undo();
    editor->SetFlowDirection(FlowDirection::LeftToRight);
    prepare();
    Check(editor->OnMouseDown(host, point, false, 0u), "task source replacement press");
    editor->SetText(L"- [ ] replacement\n");
    const auto replacementRevision = editor->Revision();
    editor->OnMouseUp(host, point, false, 0u);
    Check(editor->GetText() == L"- [ ] replacement\n" && editor->Revision() == replacementRevision,
          "source replacement cancels task release without a second edit");
    editor->SetText(L"- [ ] task\n");
    prepare();
    editor->SetSelection({6u, 6u});
    const auto staged = BuildMarkdownCommand(
        editor->Snapshot(), editor->Revision(), editor->GetMarkdownSyntax(), editor->GetSelection(), {.command = MarkdownCommand::ToggleTask});
    editor->SetSelection({7u, 7u});
    RichSemanticTests::Reject<std::invalid_argument>([&] { editor->ApplyMarkdownCommand(staged); }, "retained stale selection rejects plan");
    editor->SetReadOnly(true);
    request         = {};
    request.command = MarkdownCommand::ToggleTask;
    Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Unavailable, "readonly rejects structural commands");
    editor->SetReadOnly(false);
    editor->SetEnabled(false);
    Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Unavailable, "disabled rejects structural commands");
    editor->SetEnabled(true);
    prepare();
    editor->SetViewMode(ViewMode::Source);
    Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Unavailable, "source mode keeps structural commands opt-in");
    editor->SetViewMode(ViewMode::Rich);
    prepare();
    editor->SetText(L"pending");
    editor->SetSelection({7u, 7u});
    Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Unavailable, "unpublished analysis is never synchronously reparsed by a command");
    Check(editor->OnChar(host, L'\r', 0u) && editor->GetText() == L"pending\r\n", "pending semantic analysis preserves hardware Enter via raw source fallback");
    editor->SetText(L"- [ ] task");
    prepare();
    editor->SetSelection({8u, 8u});
    editor->SetOnTextChanged([&] { editor->Undo(); });
    Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Changed && editor->GetText() == L"- [ ] task",
          "reentrant undo sees a complete structural transaction");
    editor->SetOnTextChanged({});
    prepare();
    editor->SetOnTextChanged([&] { host.SetRoot(std::make_unique<Label>(L"replacement")); });
    Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Changed && dynamic_cast<Label*>(host.GetRoot()),
          "structural command callback can retire its owner without a stale continuation");
    prepare();
}
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
template <typename Prepare> void TaskCheckboxAlignment(ControlHost& host, Prepare&& prepare)
{
    const std::wstring source = L"- [ ] An unchecked task\r\n- [x] A checked task\r\n";
    auto root                 = std::make_unique<SourceCaretEditorFixture>(source);
    auto* editor              = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    host.SetRoot(std::move(root));
    const auto second = source.find(L"[x]");
    for (const auto flow : {FlowDirection::LeftToRight, FlowDirection::RightToLeft})
    {
        editor->SetFlowDirection(flow);
        prepare();
        const auto first = editor->TryGetTextInputRangeRects(host, 2u, 5u);
        const auto other = editor->TryGetTextInputRangeRects(host, second, second + 3u);
        const auto body  = editor->TryGetTextInputCaretRect(host, 6u);
        Check(first && first->size() == 1u && other && other->size() == 1u && body, "both hosts publish checkbox and body source geometry");
        Check(std::abs(first->front().left - other->front().left) < .01f && std::abs(first->front().right - other->front().right) < .01f,
              "native/embedded checked and unchecked tasks share the same checkbox column");
        const auto point = D2D1::Point2F((first->front().left + first->front().right) * .5f, (first->front().top + first->front().bottom) * .5f);
        Check(editor->OnMouseDown(host, point, false, 0u) && editor->GetText() == source, "task press stages without editing raw source");
        Check(editor->OnMouseUp(host, point, false, 0u), "task release checks the existing atomic source marker");
        prepare();
        auto checkedSource = source;
        checkedSource[3u]  = L'x';
        Check(editor->GetText() == checkedSource, "checkbox geometry retains the exact task click target");
        const auto after     = editor->TryGetTextInputRangeRects(host, 2u, 5u);
        const auto afterBody = editor->TryGetTextInputCaretRect(host, 6u);
        Check(after && after->size() == 1u && afterBody && std::abs(after->front().left - first->front().left) < .01f &&
                  std::abs(after->front().right - first->front().right) < .01f && std::abs(afterBody->left - body->left) < .01f &&
                  std::abs(afterBody->top - body->top) < .01f,
              "checking a task preserves native/embedded checkbox, bullet column and body placement");
        Check(editor->OnMouseDown(host, point, false, 0u) && editor->OnMouseUp(host, point, false, 0u), "the same physical target unchecks the task");
        prepare();
        Check(editor->GetText() == source && editor->Undo() && editor->GetText() == checkedSource && editor->Undo() && editor->GetText() == source,
              "check/uncheck remain separate exact-source undo transactions");
        prepare();
    }
}
template <typename Prepare> void HangingMarkersLinksAndHtml(ControlHost& host, Prepare&& prepare)
{
    const std::wstring source = L"9. first list item wraps naturally with [link](local)\r\n10. second list item\r\n\r\n<p><b>Native</b> <a href='#target' "
                                L"title='hint'>label</a><br/>&fjlig;</p>\r\n";
    auto root                 = std::make_unique<SourceCaretEditorFixture>(source);
    auto* editor              = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    host.SetRoot(std::move(root));
    prepare();
    const auto document = editor->GetSemanticDocument();
    const auto revision = editor->Revision();
    Check(document && editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && document->Map().Source() == source,
          "native/embedded owner publishes native HTML and marker geometry against exact source");
    const auto tag = source.find(L"<a"), label = source.find(L"label"), entity = source.find(L"&fjlig;");
    Check(editor->TryGetTextInputRangeRects(host, tag, label)->empty(), "native/embedded validated HTML destination attributes own no source glyphs");
    const auto rectangles = editor->TryGetTextInputRangeRects(host, label, label + 5u);
    Check(rectangles && ! rectangles->empty(), "native HTML link label has acknowledged native source rectangles");
    const auto caret = editor->TryGetTextInputCaretRect(host, label);
    Check(caret.has_value(), "link text has a real source ACP caret");
    const auto point = D2D1::Point2F(caret->left + .2f, caret->top + (caret->bottom - caret->top) * .5f);
    Check(editor->OnMouseDown(host, point, false, 0u), "click native HTML link label");
    static_cast<void>(editor->OnMouseUp(host, point, false, 0u));
    prepare();
    Check(editor->GetSelection().caret == label && editor->Revision() == revision && editor->GetText() == source && ! editor->CanUndo(),
          "link click is exact caret placement without navigation or source/history mutation");
    editor->SetSelection({entity + 2u, entity + 2u});
    Check(editor->OnKeyDown(host, VK_DELETE, 0u) && editor->GetText() == source.substr(0u, entity) + source.substr(entity + 7u),
          "native HTML reference deletion owns the complete original token");
    Check(editor->Undo() && editor->GetText() == source, "one undo restores HTML entity spelling and tags");
    prepare();
    const auto lineBreak = source.find(L"<br/>");
    editor->SetSelection({lineBreak + 1u, lineBreak + 1u});
    Check(editor->OnKeyDown(host, VK_DELETE, 0u) && editor->GetText() == source.substr(0u, lineBreak) + source.substr(lineBreak + 5u),
          "native HTML break deletion owns its complete original tag");
    Check(editor->Undo() && editor->GetText() == source, "one undo restores the native HTML break tag");
    prepare();
    editor->SetSelection({label, label});
    TextInputState original;
    Check(editor->ExportTextInputState(original) && original.text == source, "text transport exports exact HTML source, not resolved display");
    auto preview = original;
    preview.text.insert(label, L"\u6771\u4eac");
    preview.caretIndex = label + 2u;
    Check(editor->ImportTextInputState(host, preview, false), "Unicode preview in native HTML link source");
    prepare();
    Check(! editor->CanUndo() && editor->GetText() == preview.text &&
              editor->GetSemanticDocument()->Map().Display().find(L"\u6771\u4eaclabel") != std::wstring_view::npos,
          "transient HTML label preview preserves syntax and carries no history");
    Check(editor->ImportTextInputState(host, original, false), "cancel HTML source preview");
    prepare();
    Check(editor->GetText() == source && ! editor->CanUndo(), "cancel restores source-exact HTML without history");
    Check(editor->ImportTextInputState(host, preview, true), "commit Unicode HTML source preview");
    prepare();
    Check(editor->Undo() && editor->GetText() == source && ! editor->CanUndo(), "one undo restores source and HTML boundaries after commit");
    prepare();
    Check(editor->Redo() && editor->GetText() == preview.text, "redo reapplies the one raw source transaction");
    prepare();
    editor->Undo();
    prepare();
    editor->SetReadOnly(true);
    const auto unchanged = editor->GetText();
    static_cast<void>(editor->OnChar(host, L'X', 0u));
    Check(editor->GetText() == unchanged, "read-only native HTML rejects typing");
    editor->SetReadOnly(false);
    editor->SetEnabled(false);
    static_cast<void>(editor->OnChar(host, L'X', 0u));
    Check(editor->GetText() == unchanged, "disabled native HTML rejects typing");
    editor->SetEnabled(true);
    editor->SetFlowDirection(FlowDirection::RightToLeft);
    prepare();
    Check(! editor->TryGetTextInputRangeRects(host, label, label + 5u)->empty(), "RTL reflow retains native HTML raw source geometry");
    editor->SetVisible(false);
    prepare();
    Check(! editor->DebugGetState().layoutReady, "hidden native HTML retires marker and body SDK layouts");
    editor->SetVisible(true);
    prepare();
    editor->SetViewMode(ViewMode::Source);
    prepare();
    Check(! editor->TryGetTextInputRangeRects(host, tag, tag + 2u)->empty(), "source mode displays original native HTML tags");
    editor->SetViewMode(ViewMode::Rich);
    editor->SetFlowDirection(FlowDirection::LeftToRight);
    editor->SetText(L"<p style='color:red'>**literal**</p>\n");
    Check(! editor->TryGetTextInputCaretRect(host, 1u), "source replacement rejects stale native HTML geometry immediately");
    prepare();
    Check(editor->GetSemanticDocument()->Map().Display() == editor->GetText() && document->Map().Source() == source,
          "unsupported CSS keeps the whole leaf literal and old CPU publications remain immutable");
    editor->SetText(L"<p><b></b></p>\n");
    prepare();
    const auto empty = editor->TryGetTextInputCaretRect(host, 6u);
    Check(empty.has_value(), "empty native HTML has an interior insertion site in both hosts");
    const auto emptyPoint = D2D1::Point2F(empty->left, empty->top + (empty->bottom - empty->top) * .5f);
    Check(editor->OnMouseDoubleClick(host, emptyPoint, false, 0u) && editor->GetSelection() == Selection{6u, 6u} && ! editor->CanUndo(),
          "empty native HTML double-click preserves exact source insertion/history");
    Check(editor->OnChar(host, L'X', 0u) && editor->GetText() == L"<p><b>X</b></p>\n",
          "typing at the empty native HTML site preserves both tag pairs and the raw source insertion");
    prepare();
    Check(editor->GetSemanticDocument()->Map().Display() == L"X\n" && editor->Undo() && editor->GetText() == L"<p><b></b></p>\n",
          "empty native HTML typing resolves normally and is exactly one source undo");
    prepare();
}
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
        Check(editor->OnMouseDoubleClick(host, D2D1::Point2F(caret->left, (caret->top + caret->bottom) * .5f), false, 0u) &&
                  editor->GetSelection() == Selection{line.insertion, line.insertion},
              "native/embedded double-click on a caret-only source line never selects a neighboring word");
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
    Check(editor->OnMouseDoubleClick(host, D2D1::Point2F(second->left + 1.0f, (second->top + second->bottom) * .5f), false, 0u) &&
              editor->GetSelection() == Selection{anchor, anchor},
          "native/embedded empty-cell double-click remains collapsed at the real raw row-end ACP");
    const auto doubleClicked = editor->TryGetTextInputCaretRect(host, anchor);
    Check(doubleClicked && std::abs(doubleClicked->left - second->left) < .1f, "empty-cell double-click preserves equal-ACP visual affinity");
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
    std::wstring markers;
    for (unsigned item = 0u; item < 2049u; ++item)
        markers += L"- text\n";
    editor->SetText(markers);
    prepare();
    Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::SourceFallback && editor->DebugGetState().layoutReady &&
              editor->GetText() == markers && ! editor->CanUndo(),
          "admitted CPU paragraphs fall back once when native marker plus body layouts exceed capacity");
    const auto markerAdvances = editor->DebugGetState().preparationAdvances;
    prepare();
    Check(editor->DebugGetState().preparationAdvances == markerAdvances, "marker/body admission fallback has no idle retry loop");
    editor->SetText(L"- admitted\n");
    prepare();
    Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic, "new source retries marker/body admission normally");
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
