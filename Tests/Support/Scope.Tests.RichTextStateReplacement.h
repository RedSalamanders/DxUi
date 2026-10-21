#pragma once
#include <DxUi/DxUi.h>
#include <DxUi/RichDocument.h>
#include <string_view>

namespace RichTextStateReplacementTests
{
using namespace DxUi;
using namespace DxUi::RichText;
struct Example
{
    std::wstring_view before, after;
    std::size_t begin, previousEnd, currentEnd;
};
inline constexpr Example examples[] = {{L"prefix \u03b2 body suffix\r\n", L"prefix \u03b2 edit suffix\r\n", 9u, 13u, 13u},
                                       {L"a\U0001f600z", L"a\U0001f601z", 1u, 3u, 3u},
                                       {L"a\r\nb", L"a\nb", 1u, 3u, 2u},
                                       {L"ab", L"a\u03b2b", 1u, 1u, 2u},
                                       {L"a\u03b2b", L"ab", 1u, 2u, 1u},
                                       {L"ae\u0301z", L"ao\u0301z", 1u, 2u, 2u},
                                       {L"abc", L"XYZ", 0u, 3u, 3u},
                                       {L"abc", L"", 0u, 3u, 0u},
                                       {L"", L"\u03b2\U0001f600\r\n", 0u, 0u, 5u},
                                       {L"abc", L"abc", 0u, 0u, 0u}};
template <typename Check> inline void Core(Check&& check)
{
    for (const auto& example : examples)
    {
        Document document(example.before);
        const Selection originalSelection{0u, example.before.size()}, afterSelection{example.after.size(), 0u};
        document.SetSelection(originalSelection);
        const auto original = document.Snapshot();
        const auto revision = document.Revision();
        std::wstring owned(example.after);
        const bool changed = document.ReplaceWholeText(owned, afterSelection);
        check(changed == (example.before != example.after) && document.Text() == example.after && document.GetSelection() == afterSelection &&
                  original.Text() == example.before,
              "recorded complete-state replacement preserves exact Unicode spelling, selection and immutable old source");
        if (changed)
        {
            const auto delta = document.Snapshot().ChangedRangeFrom(original);
            check(delta.kind == SourceChangeKind::TrackedEdits && delta.begin == example.begin && delta.previousEnd == example.previousEnd &&
                      delta.currentEnd == example.currentEnd,
                  "complete-state replacement exposes the independently specified scalar/CRLF-safe local change interval");
            check(document.Revision() > revision && document.CanUndo() && ! document.CanRedo() && document.Undo() && document.Text() == example.before &&
                      document.GetSelection() == originalSelection,
                  "one complete-state commit is one undo transaction with the original selection");
            check(document.Redo() && document.Text() == example.after && document.GetSelection() == afterSelection,
                  "recorded replacement redo restores text and reversed selection");
        }
        else
            check(document.Revision() == revision && document.Snapshot().IsSameRevision(original) && ! document.CanUndo(),
                  "equal complete text acknowledges selection without source/history allocation or revision changes");
        owned.assign(L"caller buffer retired");
        check(document.Text() == example.after, "localized recording owns inserted source after the caller buffer changes");
    }
    Document document(L"base");
    document.SetSelection({4u, 4u});
    document.ReplaceSelection(L"!");
    document.Undo();
    const auto original = document.Snapshot();
    const auto revision = document.Revision();
    check(! document.ReplaceWholeText(L"base", {2u, 2u}) && document.CanRedo() && ! document.CanUndo() && document.Revision() == revision,
          "same-content state import preserves a pending redo history");
    document.SetSelection({4u, 4u});
    document.ReplaceTransient(L"base\u03b2", {5u, 5u});
    document.ReplaceTransient(L"base", {4u, 4u});
    check(document.CanRedo() && ! document.CanUndo() && document.Text() == L"base", "preview and rollback keep pre-composition history intact");
    check(document.ReplaceWholeText(L"base\u03b2", {5u, 5u}) && ! document.CanRedo() && document.Undo() && document.Text() == original.Text() &&
              ! document.CanUndo(),
          "notifying composition commit replaces redo with exactly one ordinary undo transaction");
    const auto stable         = document.Snapshot();
    const auto selection      = document.GetSelection();
    const auto stableRevision = document.Revision();
    const auto reject         = [&](std::wstring_view text, Selection after)
    {
        bool rejected{};
        try
        {
            document.ReplaceWholeText(text, after);
        }
        catch (const std::logic_error&)
        {
            rejected = true;
        }
        check(rejected && document.Snapshot().IsSameRevision(stable) && document.GetSelection() == selection && document.Revision() == stableRevision &&
                  document.CanRedo() && ! document.CanUndo(),
              "invalid complete service state preserves source/selection/revision and both histories");
    };
    reject(std::wstring(1u, wchar_t(0xd800)), {});
    reject(std::wstring(Document::MaximumUnits + 1u, L'x'), {});
    reject(L"\U0001f600", {1u, 1u});
    reject(L"\r\n", {1u, 1u});
    reject(L"x", {2u, 2u});
}

// The production adapter remains protected. Tests exercise service commits
// without asking the desktop clipboard, OS focus or a real IME to participate.
class ServiceEditor final : public RichTextEditor
{
public:
    using RichTextEditor::ExportTextInputState;
    using RichTextEditor::ImportTextInputState;
    using RichTextEditor::RichTextEditor;
};
template <typename Check> inline void Service(ServiceEditor& editor, ControlHost& host, Check&& check)
{
    for (const auto& example : examples)
    {
        editor.SetText(std::wstring(example.before));
        editor.SetSelection({0u, example.before.size()});
        const auto original = editor.Snapshot();
        TextInputState state{.text = std::wstring(example.after), .selectionAnchorIndex = example.after.size(), .caretIndex = 0u, .multiline = true};
        check(editor.ImportTextInputState(host, state, true) && editor.GetText() == example.after &&
                  editor.GetSelection() == Selection{example.after.size(), 0u},
              "native/embedded notifying adapter imports exact complete text and source selection");
        if (example.before != example.after)
        {
            const auto delta = editor.Snapshot().ChangedRangeFrom(original);
            check(delta.kind == SourceChangeKind::TrackedEdits && delta.begin == example.begin && delta.previousEnd == example.previousEnd &&
                      delta.currentEnd == example.currentEnd,
                  "notifying adapters keep localized source-change metadata, including supplementary scalars and CRLF edits");
            check(editor.Undo() && editor.GetText() == example.before && ! editor.CanUndo() && editor.Redo() && editor.GetText() == example.after,
                  "service-state commit is one reversible parent-document transaction");
        }
        else
            check(! editor.CanUndo() && editor.Snapshot().IsSameRevision(original), "same-content notifying import consumes no undo entry or source revision");
    }
}
} // namespace RichTextStateReplacementTests
