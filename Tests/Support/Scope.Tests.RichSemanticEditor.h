#pragma once
#include "Scope.Tests.RichSemantic.h"
#include <DxUi/DxUi.h>

namespace RichSemanticEditorTests
{
using namespace DxUi;
using namespace DxUi::RichText;
using RichSemanticTests::Check;
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
