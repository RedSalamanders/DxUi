#pragma once
#include "Controls/DxUi.PreparedMath.h"
#include "Controls/DxUi.RichSemanticGeometry.h"
#include "Controls/DxUi.RichSourceWindow.h"
#include "Support.Tests.CountingWriteFactory.h"
#include "Support.Tests.RichSemantic.h"
#include <DxUi/DxUi.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <new>
#include <wil/resource.h>
#include <wil/result.h>

namespace RichSemanticEditorTests
{
using namespace DxUi;
using namespace DxUi::RichText;
using RichSemanticTests::Check;
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class DirectPrepareRichTextEditor final : public RichTextEditor
{
public:
    using RichTextEditor::RichTextEditor;
    HRESULT PrepareOnce(ControlHost& host) noexcept
    {
        return PrepareContent(host);
    }
};
#pragma warning(pop)
inline HRESULT PrepareUntilSettled(DirectPrepareRichTextEditor& editor, ControlHost& host)
{
    for (std::size_t turn = 0u; turn < 1024u; ++turn)
    {
        const auto result = editor.PrepareOnce(host);
        if (result != S_FALSE)
            return result;
    }
    return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
}
#if DXUI_ENABLE_DIAGNOSTICS
class SemanticWriteFactoryOverride final
{
public:
    explicit SemanticWriteFactoryOverride(ControlHost& host) : previous_(RichText::Detail::testRichSemanticWriteFactory)
    {
        const auto wrapped = Microsoft::WRL::Make<NativeTextTests::CountingWriteFactory>(host.GetWriteFactory());
        if (! wrapped)
            throw std::bad_alloc();
        wrapped_                                       = wrapped.Get();
        RichText::Detail::testRichSemanticWriteFactory = wrapped_.get();
    }
    SemanticWriteFactoryOverride(const SemanticWriteFactoryOverride&)            = delete;
    SemanticWriteFactoryOverride& operator=(const SemanticWriteFactoryOverride&) = delete;
    SemanticWriteFactoryOverride(SemanticWriteFactoryOverride&&)                 = delete;
    SemanticWriteFactoryOverride& operator=(SemanticWriteFactoryOverride&&)      = delete;
    ~SemanticWriteFactoryOverride()
    {
        RichText::Detail::testRichSemanticWriteFactory = previous_;
    }
    NativeTextTests::CountingWriteFactory* Get() const noexcept
    {
        return wrapped_.get();
    }

private:
    wil::com_ptr<NativeTextTests::CountingWriteFactory> wrapped_;
    IDWriteFactory* previous_{};
};
#endif
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
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
#pragma warning(pop)
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
    unsigned menus = 0u;
    const RichTextContextMenuLabels labels{L"Undo", L"Redo", L"Cut", L"Copy", L"Paste", L"Select all", L"Bold", L"Italic"};
    editor->SetDefaultContextMenuLabels(&labels);
    auto invalidLabels = labels;
    invalidLabels.cut.clear();
    RichSemanticTests::Reject<std::invalid_argument>([&] { editor->SetDefaultContextMenuLabels(&invalidLabels); },
                                                     "default context menu requires complete localized labels");
    editor->SetOnContextMenu([&](POINT, bool) { ++menus; });
    editor->SetSelection({0u, 2u});
    std::vector<MenuFlyoutItem> menuItems(0u);
    editor->BuildDefaultContextMenuItems(menuItems);
    Check(menuItems.size() == 8u && menuItems[2u].enabled && menuItems[3u].enabled && menuItems[2u].text == L"Cut",
          "embedded clients can obtain the localized default menu model");
    Check(editor->InvokeDefaultContextMenuCommand(host, static_cast<int>(RichTextContextMenuCommand::SelectAll)) &&
              editor->GetSelection() == Selection{0u, 4u} && ! editor->InvokeDefaultContextMenuCommand(host, -1),
          "embedded clients can dispatch a default menu action and reject unknown commands");
    editor->SetSelection({0u, 2u});
    Check(editor->OnContextMenu(host, true, D2D1::Point2F(8.0f, 8.0f)) && menus == 1u && editor->GetSelection() == Selection{0u, 2u},
          "keyboard context menu preserves selection and reaches the consumer callback");
    Check(editor->OnContextMenu(host, false, D2D1::Point2F(450.0f, 15.0f)) && menus == 2u && editor->GetSelection() == Selection{4u, 4u},
          "pointer context menu outside the selection moves the caret before invoking its callback");
    editor->SetOnContextMenu([](POINT, bool) { throw std::runtime_error("test observer failure"); });
    Check(editor->OnContextMenu(host, true, D2D1::Point2F(8.0f, 8.0f)), "failed context observer is delivered once and cannot fall back to a second menu");
    editor->SetOnContextMenu({});
    editor->SetDefaultContextMenuLabels(nullptr);
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
    editor->SetText(L"One one only");
    const auto searchSource  = editor->Snapshot();
    const auto searchMatches = FindInSource(searchSource, L"one", {.wholeWord = true});
    editor->SetSearchMatches(searchSource, searchMatches, 1u);
    Check(editor->GetSourceHighlights().size() == 2u && ! editor->GetSourceHighlights()[0].current && editor->GetSourceHighlights()[1].current,
          "revision-guarded search matches feed existing highlight geometry with an active match");
    Check(editor->SelectSourceMatch(L"one", false, {.wholeWord = true}) && editor->GetSelection() == Selection{0u, 3u} &&
              editor->SelectSourceMatch(L"one", false, {.wholeWord = true}) && editor->GetSelection() == Selection{4u, 7u} &&
              editor->SelectSourceMatch(L"one", false, {.wholeWord = true}) && editor->GetSelection() == Selection{0u, 3u} &&
              editor->SelectSourceMatch(L"one", true, {.wholeWord = true}) && editor->GetSelection() == Selection{4u, 7u},
          "source match navigation wraps in both directions");
    unsigned findRequests = 0u;
    editor->SetSearchQuery(L"one", {.wholeWord = true});
    editor->SetOnFindRequested([&] { ++findRequests; });
    editor->SetSearchShortcutsEnabled(true);
    Check(editor->OnKeyDown(host, 'F', MK_CONTROL) && findRequests == 1u && editor->OnKeyDown(host, VK_F3, 0u) && editor->GetSelection() == Selection{0u, 3u} &&
              editor->OnKeyDown(host, VK_F3, MK_SHIFT) && editor->GetSelection() == Selection{4u, 7u},
          "opt-in find and next/previous shortcuts route through current search state");
    editor->SetSearchShortcutsEnabled(false);
    Check(editor->ReplaceSelectedSourceMatch(L"one", L"two", {.wholeWord = true}) && editor->GetText() == L"One two only" && editor->Undo() &&
              editor->GetText() == L"One one only",
          "replace selected match is one source-exact undo transaction");
    Check(editor->ReplaceAllSourceMatches(L"one", L"x", {.wholeWord = true}) == 2u && editor->GetText() == L"x x only" && editor->Undo() &&
              editor->GetText() == L"One one only",
          "editor replace-all is case-insensitive and one undo transaction");
    const std::wstring nulSource(L"a\0ba\0b", 6u);
    editor->SetText(nulSource);
    editor->SetSelection({0u, 0u});
    editor->SetSearchQuery(std::wstring_view(L"\0b", 2u));
    editor->SetSearchShortcutsEnabled(true);
    Check(editor->OnKeyDown(host, VK_F3, 0u) && editor->GetSelection() == Selection{1u, 3u} && editor->OnKeyDown(host, VK_F3, 0u) &&
              editor->GetSelection() == Selection{4u, 6u},
          "stored counted-NUL search queries retain their exact extent through next-match shortcuts");
    editor->SetSearchShortcutsEnabled(false);
    Check(editor->ReplaceAllSourceMatches(std::wstring_view(L"\0b", 2u), std::wstring_view(L"\0", 1u)) == 2u &&
              editor->GetText() == std::wstring(L"a\0a\0", 4u) && editor->Undo() && editor->GetText() == nulSource,
          "counted-NUL queries and replacements preserve exact source and one undo transaction");
    editor->SetText(L"word");
    const auto decorationSource = editor->Snapshot();
    const std::array decorations{RichTextDecoration{0u, 4u, RichTextDecorationKind::Background, RichTextDecorationRole::Accent},
                                 RichTextDecoration{0u, 4u, RichTextDecorationKind::Squiggle, RichTextDecorationRole::Error}};
    editor->SetDecorations(decorationSource, decorations);
    Check(editor->GetDecorationStatus() == RichTextDecorationStatus::Pending && editor->GetDecorations().size() == decorations.size(),
          "revision-local annotations can be supplied before the first current layout");
    prepare();
    Check(editor->GetDecorationStatus() == RichTextDecorationStatus::Ready, "pending annotations publish after current layout preparation");
    editor->SetSelection({0u, 4u});
    const std::array highlights{RichTextSourceHighlight{0u, 2u, true}};
    editor->SetSourceHighlights(highlights);
    const std::vector<RichTextDecoration> tooManyDecorations(257u);
    RichSemanticTests::Reject<std::length_error>([&] { editor->SetDecorations(decorationSource, tooManyDecorations); },
                                                 "decoration count is bounded before staging");
    Check(editor->GetDecorations().size() == decorations.size(), "rejected decoration replacement preserves current state");
    Check(editor->DebugGetState().overlayGeometryPending, "selection and highlight changes invalidate prepared overlay geometry");
    prepare();
    Check(! editor->DebugGetState().overlayGeometryPending && editor->DebugGetState().preparedSelectionRectangles != 0u &&
              editor->DebugGetState().preparedHighlightRectangles != 0u && editor->DebugGetState().preparedDecorationRectangles >= 2u,
          "selection and search rectangles are prepared before composition");
    Check(editor->GetDecorationStatus() == RichTextDecorationStatus::Ready, "accepted decoration geometry publishes ready status");
#if DXUI_ENABLE_DIAGNOSTICS
    editor->DebugSetDecorationRectangleLimitForTest(1u);
    prepare();
    Check(editor->GetDecorationStatus() == RichTextDecorationStatus::CapacityExceeded && editor->GetDecorations().size() == 2u &&
              editor->GetText() == L"word" && editor->DebugGetState().preparedDecorationRectangles == 0u,
          "reflow capacity rejection retains requests and leaves the document prepared without stale decoration geometry");
    auto oversized    = decorations;
    oversized[0].role = RichTextDecorationRole::Warning;
    RichSemanticTests::Reject<std::length_error>([&] { editor->SetDecorations(decorationSource, oversized); },
                                                 "setter rejects geometry above the current ceiling before publication");
    Check(editor->GetDecorations()[0].role == RichTextDecorationRole::Accent, "rejected replacement preserves the previous decoration request");
    editor->DebugSetDecorationRectangleLimitForTest(4096u);
    prepare();
    Check(editor->GetDecorationStatus() == RichTextDecorationStatus::Ready, "a later admissible geometry restores retained decorations");
#endif
    editor->ClearSourceHighlights();
    prepare();
    Check(editor->DebugGetState().preparedHighlightRectangles == 0u, "cleared highlights retire their prepared rectangles");
    editor->SetText(L"");
    Check(editor->GetDecorations().empty(), "source replacement retires revision-local decorations");
    RichSemanticTests::Reject<std::invalid_argument>([&] { editor->SetDecorations(decorationSource, decorations); }, "stale decoration source is rejected");
    Check(! editor->IsAutoPairEnabled() && editor->OnChar(host, L'(', 0u) && editor->GetText() == L"(" && editor->Undo(),
          "direct typing keeps literal punctuation by default");
    editor->SetAutoPairEnabled(true);
    Check(editor->OnChar(host, L'(', 0u) && editor->GetText() == L"()" && editor->GetSelection() == Selection{1u, 1u} && editor->Undo() &&
              editor->GetText().empty(),
          "opt-in direct punctuation inserts one paired undo transaction with an interior caret");
    editor->SetText(L"word");
    editor->SetSelection({4u, 0u});
    Check(editor->OnChar(host, L'[', 0u) && editor->GetText() == L"[word]" && editor->GetSelection() == Selection{5u, 1u} && editor->Undo() &&
              editor->GetText() == L"word",
          "opt-in pairing wraps a reversed selection without changing direction and undoes atomically");
    editor->SetReadOnly(true);
    Check(! editor->OnChar(host, L'{', 0u) && editor->GetText() == L"word", "read-only input cannot auto-pair");
    editor->SetReadOnly(false);
    editor->SetAutoPairEnabled(false);
    editor->SetText(L"");
    prepare();
    Check(! editor->IsSmartTypographyEnabled(), "smart typography is disabled by default");
    editor->SetSmartTypographyEnabled(true);
    Check(editor->OnChar(host, L'.', 0u) && editor->OnChar(host, L'.', 0u) && editor->OnChar(host, L'.', 0u) && editor->GetText() == L"\u2026" &&
              editor->Undo() && editor->GetText() == L"..",
          "rapid ellipsis typing remains one replacement undo even before semantic analysis republishes");
    editor->SetText(L"");
    prepare();
    Check(editor->OnChar(host, L'-', 0u) && editor->OnChar(host, L'-', 0u) && editor->OnChar(host, L'-', 0u) && editor->GetText() == L"\u2014" &&
              editor->Undo() && editor->GetText() == L"\u2013",
          "direct dash replacement is atomic and upgrades an en dash to an em dash");
    editor->SetText(L"`x`");
    prepare();
    editor->SetSelection({2u, 2u});
    Check(editor->OnChar(host, L'"', 0u) && editor->GetText() == L"`x\"`", "smart quotes leave inline code spelling literal");
    editor->SetText(L"");
    prepare();
    Check(editor->OnChar(host, L'"', 0u) && editor->GetText() == L"\u201c", "smart quote opens at the start of prose");
    editor->SetText(L"\\(x\\)");
    prepare();
    editor->SetSelection({3u, 3u});
    Check(editor->OnChar(host, L'.', 0u), "seed a smart-typing site under the previous Markdown profile");
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    Check(editor->OnChar(host, L'"', 0u) && editor->GetText() == L"\\(x.\"\\)",
          "a profile switch retires a cached prose site before the new math analysis publishes");
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetSmartTypographyEnabled(false);
    editor->SetText(L"");
    prepare();
    Check(! editor->IsMarkdownAutoFormatEnabled() && editor->OnChar(host, L'#', 0u) && editor->OnChar(host, L' ', 0u) && editor->Undo() &&
              editor->GetText() == L"#",
          "Markdown trigger coalescing is disabled by default");
    editor->SetMarkdownAutoFormatEnabled(true);
    editor->SetText(L"");
    prepare();
    Check(editor->OnChar(host, L'#', 0u), "seed a pending Markdown trigger");
    editor->SetViewMode(ViewMode::Source);
    editor->SetViewMode(ViewMode::Rich);
    Check(editor->OnChar(host, L' ', 0u) && editor->Undo() && editor->GetText() == L"#",
          "view transitions retire pending trigger grouping even if the source revision is unchanged");
    editor->SetText(L"");
    prepare();
    Check(editor->OnChar(host, L'#', 0u), "seed a trigger before an explicit command");
    static_cast<void>(editor->OnKeyDown(host, VK_ESCAPE, 0u));
    Check(editor->OnChar(host, L' ', 0u) && editor->Undo() && editor->GetText() == L"#",
          "Escape ends pending trigger grouping without requiring a selection change");
    for (const auto trigger :
         {std::wstring_view(L"## "), std::wstring_view(L"- "), std::wstring_view(L"1. "), std::wstring_view(L"> "), std::wstring_view(L"---")})
    {
        editor->SetText(L"");
        prepare();
        for (const auto character : trigger)
            Check(editor->OnChar(host, character, 0u), "direct Markdown trigger character is handled");
        if (trigger == L"---")
        {
            prepare();
            Check(
                std::ranges::any_of(editor->GetMarkdownSyntax()->Nodes(), [](const auto& node) { return node.block.kind == MarkdownBlockKind::ThematicBreak; }),
                "thematic trigger reparses to a real rule");
        }
        Check(editor->GetText() == trigger && editor->Undo() && editor->GetText().empty(),
              "a complete top-level Markdown trigger is one source-exact undo transaction");
    }
    editor->SetText(L"");
    prepare();
    Check(editor->OnChar(host, L'`', 0u) && editor->OnChar(host, L'`', 0u) && editor->OnChar(host, L'`', 0u) && editor->GetText() == L"```\r\n\r\n```" &&
              editor->GetSelection() == Selection{5u, 5u},
          "a fenced-code trigger creates a closed editable block in one undo step");
    prepare();
    Check(std::ranges::any_of(editor->GetMarkdownSyntax()->Nodes(),
                              [](const auto& node) { return node.block.kind == MarkdownBlockKind::FencedCode && node.block.closed; }) &&
              editor->Undo() && editor->GetText().empty(),
          "fence trigger publishes a closed code block and undoes atomically");
    for (const auto source : {std::wstring_view(L"> \n"), std::wstring_view(L"- \n")})
    {
        editor->SetText(std::wstring(source));
        prepare();
        editor->SetSelection({2u, 2u});
        Check(editor->OnChar(host, L'#', 0u) && editor->OnChar(host, L' ', 0u) && editor->GetText() == std::wstring(source.substr(0u, 2u)) + L"# \n" &&
                  editor->Undo() && editor->GetText() == source,
              "an empty quote or list item admits an atomic nested heading trigger");
    }
    for (const auto& sample : {std::pair{std::wstring_view(L"> \n"), std::wstring_view(L"> ```\n> \n> ```\n")},
                               std::pair{std::wstring_view(L"- \n"), std::wstring_view(L"- ```\n  \n  ```\n")},
                               std::pair{std::wstring_view(L"-\t\n"), std::wstring_view(L"-\t```\n \t\n \t```\n")},
                               std::pair{std::wstring_view(L"12. \n"), std::wstring_view(L"12. ```\n    \n    ```\n")}})
    {
        editor->SetText(std::wstring(sample.first));
        prepare();
        const auto start = sample.first.find(L'\n');
        editor->SetSelection({start, start});
        const auto bodyCaret = sample.second.find(L'\n') + 1u + start;
        Check(editor->OnChar(host, L'`', 0u) && editor->OnChar(host, L'`', 0u) && editor->OnChar(host, L'`', 0u) && editor->GetText() == sample.second &&
                  editor->GetSelection() == Selection{bodyCaret, bodyCaret},
              "nested fence trigger retains a proven continuation prefix and interior caret");
        prepare();
        Check(std::ranges::any_of(editor->GetMarkdownSyntax()->Nodes(),
                                  [](const auto& node) { return node.block.kind == MarkdownBlockKind::FencedCode && node.block.closed; }) &&
                  editor->Undo() && editor->GetText() == sample.first,
              "nested generated fence reparses as closed and undoes in one transaction");
    }
    editor->SetText(L"~~~\n\n~~~");
    prepare();
    editor->SetSelection({4u, 4u});
    Check(editor->OnChar(host, L'#', 0u) && editor->OnChar(host, L' ', 0u) && editor->Undo() && editor->GetText() == L"~~~\n#\n~~~",
          "Markdown trigger policy does not transform fenced-code input");
    editor->SetMarkdownAutoFormatEnabled(false);
    editor->SetText(L"");
    Check(editor->OnChar(host, L'a', 0u) && editor->OnChar(host, L'b', 0u) && editor->OnChar(host, L'c', 0u) && editor->GetText() == L"abc" && editor->Undo() &&
              editor->GetText().empty(),
          "adjacent direct word typing coalesces into one undo");
    Check(editor->OnChar(host, static_cast<wchar_t>(0xd83d), 0u) && editor->OnChar(host, static_cast<wchar_t>(0xde00), 0u) && editor->OnChar(host, L'a', 0u) &&
              editor->OnChar(host, L'\u0301', 0u) && editor->GetText() == L"\U0001f600a\u0301" && editor->Undo() && editor->GetText().empty(),
          "supplementary scalars and combining marks join a bounded typing group without splitting UTF-16");
    editor->SetViewMode(ViewMode::Source);
    editor->SetText(L"\U0001f600a\u0301");
    prepare();
    editor->SetSelection({4u, 4u});
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText().empty() && editor->Undo() &&
              editor->GetText() == L"\U0001f600a\u0301",
          "adjacent source grapheme deletions restore both whole elements in one undo");
    editor->SetText(L"\U0001f469\u200d\U0001f4bba");
    prepare();
    editor->SetSelection({6u, 6u});
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText().empty() && editor->Undo() &&
              editor->GetText() == L"\U0001f469\u200d\U0001f4bba",
          "joined emoji and adjacent text restore as whole graphemes in one undo");
    editor->SetViewMode(ViewMode::Rich);
    editor->SetText(L"## title\n");
    prepare();
    editor->SetSelection({3u, 3u});
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText() == L"title\n" && editor->Undo() && editor->GetText() == L"## title\n",
          "rich backspace at heading content removes the heading marker in one undo step");
    editor->SetText(L"> one\n> two\n");
    prepare();
    editor->SetSelection({2u, 2u});
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText() == L"one\ntwo\n" && editor->Undo() && editor->GetText() == L"> one\n> two\n",
          "rich backspace at the first quote leaf removes one level from every quote line in one undo step");
    editor->SetText(L"**abc**");
    prepare();
    editor->SetSelection({5u, 5u});
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText() == L"**ab**", "semantic deletion preserves a nonempty wrapper");
    prepare();
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText() == L"**a**", "adjacent semantic deletion retains the remaining wrapper");
    prepare();
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText().empty() && editor->Undo() && editor->GetText() == L"**a**" && editor->Undo() &&
              editor->GetText() == L"**abc**",
          "plain semantic erasures coalesce while structural wrapper removal remains a separate undo");
    editor->SetText(L"");
    Check(editor->OnChar(host, L'a', 0u) && editor->OnChar(host, L' ', 0u) && editor->OnChar(host, L'b', 0u) && editor->Undo() && editor->GetText() == L"a ",
          "whitespace ends a typing undo group");
    editor->SetText(L"");
    Check(editor->OnChar(host, L'a', 0u) && editor->OnChar(host, L'b', 0u), "typing fixture starts a group");
    editor->SetSelection({0u, 0u});
    editor->SetSelection({2u, 2u});
    Check(editor->OnChar(host, L'c', 0u) && editor->Undo() && editor->GetText() == L"ab",
          "selection moves break a typing group even when returning to its caret");
    editor->SetText(L"word");
    prepare();
    editor->SetDocumentFilePath(L"C:\\docs\\guide.md");
    editor->SetText(L"");
    prepare();
    MarkdownCommandRequest imageRequest;
    imageRequest.command     = MarkdownCommand::InsertImage;
    imageRequest.text        = L"photo";
    imageRequest.destination = L"C:\\docs\\pics\\photo.png";
    Check(editor->ExecuteMarkdownCommand(imageRequest) == MarkdownCommandStatus::Changed && editor->GetText() == L"![photo](<pics/photo.png>)" &&
              editor->Undo() && editor->GetText().empty(),
          "image command reuses document-relative file destination and one undo");
    editor->SetDocumentFilePath(L"");
    editor->SetText(L"word");
    prepare();
    editor->SetText(L"**word** plain");
    prepare();
    editor->SetSelection({2u, 6u});
    MarkdownCommandRequest stateRequest;
    stateRequest.command = MarkdownCommand::ToggleStyle;
    stateRequest.style   = MarkdownTextStyle::Strong;
    const auto boldState = editor->QueryMarkdownCommandState(stateRequest);
    Check(boldState && boldState->active && ! boldState->mixed && ! boldState->available,
          "command-state query reports current bold state without claiming unresolved applicability");
    editor->SetSelection({2u, 14u});
    const auto mixedState = editor->QueryMarkdownCommandState(stateRequest);
    Check(mixedState && mixedState->mixed && ! mixedState->active, "command-state query reports mixed formatting");
    editor->SetText(L"- a\n\n1. b\n\n- c\n");
    prepare();
    editor->SetSelection({0u, 15u});
    stateRequest.command  = MarkdownCommand::SetList;
    stateRequest.listKind = MarkdownListKind::Unordered;
    const auto mixedLists = editor->QueryMarkdownCommandState(stateRequest);
    Check(mixedLists && mixedLists->mixed && ! mixedLists->listKind && ! mixedLists->active,
          "list state sees an ordered interior even when both selection endpoints are unordered");
    editor->SetText(L"- outer\n  1. inner\n");
    prepare();
    editor->SetSelection({13u, 18u});
    const auto nestedList = editor->QueryMarkdownCommandState(stateRequest);
    Check(nestedList && ! nestedList->mixed && nestedList->listKind == MarkdownListKind::Ordered,
          "selection wholly inside a nested item reports its innermost list kind");
    std::wstring deepLists(0u, L' ');
    for (size_t depth = 0u; depth < 66u; ++depth)
    {
        deepLists.append(depth * 2u, L' ');
        deepLists += L"- x\n";
    }
    deepLists += L"\nroot\n";
    editor->SetText(deepLists);
    prepare();
    editor->SetSelection({0u, deepLists.size()});
    const auto deepState = editor->QueryMarkdownCommandState(stateRequest);
    Check(deepState && deepState->mixed && ! deepState->listKind,
          "deep nested list state restores ancestors beyond the fixed stack and detects the unlisted tail");
    std::wstring wrappedLists(0u, L' ');
    for (size_t depth = 0u; depth < 65u; ++depth)
    {
        wrappedLists.append(depth * 2u, L' ');
        wrappedLists += L"- x\n";
    }
    for (size_t sibling = 0u; sibling < 12u; ++sibling)
    {
        wrappedLists.append(130u, L' ');
        for (size_t quote = 0u; quote < 20u; ++quote)
            wrappedLists += L"> ";
        wrappedLists += L"- x\n";
    }
    editor->SetText(wrappedLists);
    prepare();
    editor->SetSelection({0u, wrappedLists.size()});
    const auto wrappedState  = editor->QueryMarkdownCommandState(stateRequest);
    const auto ancestorSteps = editor->DebugGetListStateAncestorSteps();
    Check(wrappedState && ! wrappedState->mixed && wrappedState->listKind == MarkdownListKind::Unordered && ancestorSteps >= 240u,
          "deep nested sibling lists exercise parent-link recovery through repeated quote wrappers");
    std::cout << "list-state source units=" << wrappedLists.size() << " ancestor steps=" << ancestorSteps << '\n';
    editor->SetText(L"intro\n\n- item\n\nending");
    prepare();
    editor->SetSelection({0u, editor->Snapshot().Length()});
    const auto gapState = editor->QueryMarkdownCommandState(stateRequest);
    Check(gapState && gapState->mixed && ! gapState->listKind, "list state includes ordinary source before and after a list");
    editor->SetText(L"- item");
    prepare();
    editor->SetSelection({0u, 2u});
    const auto prefixState = editor->QueryMarkdownCommandState(stateRequest);
    Check(prefixState && ! prefixState->mixed && prefixState->listKind == MarkdownListKind::Unordered, "list state includes the raw item prefix");
    editor->SetSelection({6u, 6u});
    const auto eofState = editor->QueryMarkdownCommandState(stateRequest);
    Check(eofState && ! eofState->mixed && eofState->listKind == MarkdownListKind::Unordered, "caret at source EOF retains the final list item's state");
    editor->SetText(L"**word** plain");
    prepare();
    editor->SetSelection({2u, 14u});
    for (const auto command : {MarkdownCommand::JoinBackward, MarkdownCommand::JoinForward})
    {
        stateRequest.command = command;
        const auto joinState = editor->QueryMarkdownCommandState(stateRequest);
        Check(joinState && joinState->available == std::optional<bool>{false} &&
                  editor->ExecuteMarkdownCommand(stateRequest) == MarkdownCommandStatus::Unavailable,
              "join state and execution both reject a noncollapsed selection");
    }
    editor->SetSelection({4u, 4u});
    for (const auto command : {MarkdownCommand::JoinBackward, MarkdownCommand::JoinForward})
    {
        stateRequest.command = command;
        const auto joinState = editor->QueryMarkdownCommandState(stateRequest);
        Check(joinState && joinState->available == std::optional<bool>{false} &&
                  editor->ExecuteMarkdownCommand(stateRequest) == MarkdownCommandStatus::Unavailable,
              "join state and execution agree that a caret inside formatted text is not a boundary");
    }
    editor->SetText(L"alone");
    prepare();
    editor->SetSelection({0u, 0u});
    stateRequest.command    = MarkdownCommand::JoinBackward;
    const auto isolatedJoin = editor->QueryMarkdownCommandState(stateRequest);
    Check(isolatedJoin && isolatedJoin->available == std::optional<bool>{false} &&
              editor->ExecuteMarkdownCommand(stateRequest) == MarkdownCommandStatus::Unavailable,
          "join state and execution reject a first paragraph without a predecessor");
    editor->SetText(L"a\n\nb");
    prepare();
    editor->SetSelection({3u, 3u});
    const auto plainJoin = editor->QueryMarkdownCommandState(stateRequest);
    Check(plainJoin && plainJoin->available == std::optional<bool>{true} && editor->ExecuteMarkdownCommand(stateRequest) == MarkdownCommandStatus::Changed &&
              editor->GetText() == L"a b" && editor->Undo() && editor->GetText() == L"a\n\nb",
          "join state and execution admit a plain paragraph boundary and undo it atomically");
    editor->SetText(L"## title\n");
    prepare();
    editor->SetSelection({3u, 3u});
    const auto headingJoin = editor->QueryMarkdownCommandState(stateRequest);
    Check(headingJoin && headingJoin->available == std::optional<bool>{true} &&
              editor->ExecuteMarkdownCommand(stateRequest) == MarkdownCommandStatus::Changed && editor->GetText() == L"title\n" && editor->Undo(),
          "join state and execution agree on removing a document-level heading");
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::CommonMark0312);
    editor->SetText(L"- a\n");
    prepare();
    editor->SetSelection({2u, 2u});
    for (const auto command : {MarkdownCommand::ToggleTask, MarkdownCommand::SetList, MarkdownCommand::InsertTable,
                               MarkdownCommand::DeleteTableColumn, MarkdownCommand::ToggleStyle})
    {
        MarkdownCommandRequest disabled;
        disabled.command = command;
        disabled.listKind = MarkdownListKind::Task;
        disabled.style = MarkdownTextStyle::Strikethrough;
        const auto disabledState = editor->QueryMarkdownCommandState(disabled);
        Check(disabledState && disabledState->available == std::optional<bool>{false} &&
                  editor->ExecuteMarkdownCommand(disabled) == MarkdownCommandStatus::Unavailable,
              "disabled dialect features are unavailable in both command state and execution");
    }
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetText(L"~~~\nx\n~~~\n");
    prepare();
    editor->SetSelection({4u, 4u});
    stateRequest.command = MarkdownCommand::JoinBackward;
    const auto codeJoin  = editor->QueryMarkdownCommandState(stateRequest);
    Check(codeJoin && codeJoin->available == std::optional<bool>{false} && editor->ExecuteMarkdownCommand(stateRequest) == MarkdownCommandStatus::Unavailable,
          "join state and execution both reject a caret in fenced code");
    stateRequest.command = MarkdownCommand::ExitCodeBlock;
    const auto exitState = editor->QueryMarkdownCommandState(stateRequest);
    Check(exitState && exitState->available == std::optional<bool>{true} && editor->ExecuteMarkdownCommand(stateRequest) == MarkdownCommandStatus::Changed &&
              editor->Undo(),
          "code-exit state uses the same admission as execution without copying the native text cache");
    editor->SetText(L"[link](https://example.invalid)");
    prepare();
    editor->SetSelection({2u, 2u});
    stateRequest.command      = MarkdownCommand::InsertEscapedText;
    stateRequest.text         = L"[x]*";
    const auto blockedLiteral = editor->QueryMarkdownCommandState(stateRequest);
    Check(blockedLiteral && blockedLiteral->available == std::optional<bool>{false} &&
              editor->ExecuteMarkdownCommand(stateRequest) == MarkdownCommandStatus::Unavailable,
          "literal command state and execution both reject an opaque link interior");
    editor->SetText(L"");
    prepare();
    const auto literalState = editor->QueryMarkdownCommandState(stateRequest);
    Check(literalState && literalState->available == std::optional<bool>{true} &&
              editor->ExecuteMarkdownCommand(stateRequest) == MarkdownCommandStatus::Changed && editor->GetText() == L"\\[x\\]\\*" && editor->Undo(),
          "literal command state admits an insertion the editor can execute and undo");
    editor->SetText(L"word");
    prepare();
    editor->SetMarkdownShortcutsEnabled(true);
    editor->SetSelection({0u, 4u});
    Check(editor->OnKeyDown(host, 'X', MK_CONTROL | MK_SHIFT) && editor->GetText() == L"~~word~~" && editor->Undo(),
          "opt-in Ctrl+Shift+X toggles semantic strikethrough");
    prepare();
    editor->SetSelection({2u, 2u});
    Check(editor->OnKeyDown(host, '2', MK_CONTROL | MK_SHIFT) && editor->GetText() == L"## word" && editor->Undo(),
          "opt-in Ctrl+Shift+2 applies a level-two heading");
    prepare();
    unsigned linkRequests = 0u;
    editor->SetOnLinkEditRequested([&] { ++linkRequests; });
    Check(editor->OnKeyDown(host, 'K', MK_CONTROL) && linkRequests == 1u, "opt-in Ctrl+K delivers one consumer link-edit request");
    editor->SetReadOnly(true);
    Check(! editor->OnKeyDown(host, 'K', MK_CONTROL) && linkRequests == 1u, "read-only Ctrl+K does not request editing");
    editor->SetReadOnly(false);
    editor->SetMarkdownShortcutsEnabled(false);
    std::size_t notifications = 0u;
    editor->SetOnTextChanged([&] { ++notifications; });
    const auto exact = [&](std::wstring_view source,
                           Selection selection,
                           MarkdownCommandRequest request,
                           std::wstring_view expected,
                           std::optional<Selection> after = std::nullopt)
    {
        editor->SetText(std::wstring(source));
        prepare();
        editor->SetSelection(selection);
        const auto old    = editor->Snapshot();
        const auto count  = notifications;
        const auto status = editor->ExecuteMarkdownCommand(request);
        if (status != MarkdownCommandStatus::Changed)
            std::wcerr << L"unexpected retained command status=" << static_cast<unsigned>(status) << L" command=" << static_cast<unsigned>(request.command)
                       << L" style=" << static_cast<unsigned>(request.style) << L" source=[" << source << L"] selection={" << selection.anchor << L','
                       << selection.caret << L"}\n";
        Check(status == MarkdownCommandStatus::Changed && editor->GetText() == expected && (! after || editor->GetSelection() == *after) &&
                  notifications == count + 1u,
              "retained command publishes complete source once in both hosts");
        Check(editor->Undo() && editor->Snapshot().IsSameRevision(old) && editor->GetSelection() == selection && ! editor->CanUndo(),
              "retained command one exact undo");
        Check(editor->Redo() && editor->GetText() == expected && (! after || editor->GetSelection() == *after) && ! editor->CanRedo(),
              "retained command one redo");
        prepare();
        Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && editor->GetSemanticDocument()->Map().Source() == expected,
              "retained command re-prepares through owner scheduling");
    };
    const auto unavailable = [&](std::wstring_view source, Selection selection, MarkdownCommandRequest request)
    {
        editor->SetText(std::wstring(source));
        prepare();
        editor->SetSelection(selection);
        const auto snapshot = editor->Snapshot();
        const auto count    = notifications;
        Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Unavailable && editor->Snapshot().IsSameRevision(snapshot) &&
                  editor->GetSelection() == selection && notifications == count && ! editor->CanUndo(),
              "retained editor rejects unsafe structural command without source or history changes");
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
    request         = {};
    request.command = MarkdownCommand::DeleteTableColumn;
    exact(L"| a | b |\n| - | -: |\n| x |\n", {2u, 2u}, request, L"| b |\n| -: |\n| |\n", Selection{2u, 2u});
    request.command = MarkdownCommand::InsertTableColumn;
    exact(L"a | b\n- | -\nx | y\n", {0u, 0u}, request, L"| | a | b\n| --- | - | -\n| | x | y\n", Selection{2u, 2u});
    request.command = MarkdownCommand::DeleteTableRow;
    exact(L"| a |\n| - |\n| x |", {2u, 2u}, request, L"| x |\n| - |\n", Selection{2u, 2u});
    exact(L"| a | b |\n| - | - |\n|😀|x|\n", {2u, 2u}, request, L"|😀|x|\n| - | - |\n", Selection{1u, 1u});
    request         = {};
    request.command = MarkdownCommand::OutdentList;
    exact(L"- a\n    - b\n", {10u, 10u}, request, L"- a\n- b\n");
    exact(L"- - inner\n", {4u, 9u}, request, L"- inner\n", Selection{2u, 7u});
    request.command = MarkdownCommand::IndentList;
    exact(L"- a\n- b\n\n  tail\n- c\n", {6u, 6u}, request, L"- a\n  - b\n\n    tail\n- c\n");
    exact(L"- parent\n  - x\n  - a\n\n    tail\n", {19u, 19u}, request, L"- parent\n  - x\n    - a\n\n      tail\n");
    request.command = MarkdownCommand::OutdentList;
    exact(L"- parent\n  - x\n  - a\n\n    tail\n", {19u, 19u}, request, L"- parent\n  - x\n- a\n\n  tail\n");
    request.command  = MarkdownCommand::SetList;
    request.listKind = MarkdownListKind::None;
    exact(L"- parent\n  - x\n  - a\n\n    tail\n", {19u, 19u}, request, L"- parent\n  - x\n  a\n\n  tail\n");
    request         = {};
    request.command = MarkdownCommand::ToggleStyle;
    request.style   = MarkdownTextStyle::Strong;
    exact(L"**Hello, world! (see e.g. foo)**", {3u, 3u}, request, L"Hello, world! (see e.g. foo)");
    request.style = MarkdownTextStyle::Code;
    exact(L"**left &amp; right**", {7u, 12u}, request, L"**left `&` right**", Selection{8u, 9u});
    request.command     = MarkdownCommand::SetLink;
    request.destination = L"next";
    exact(L"<a href='old' title='tip'>word</a>", {28u, 28u}, request, L"<a href='next'>word</a>", Selection{17u, 17u});
    request         = {};
    request.command = MarkdownCommand::InsertParagraph;
    exact(L"> ***", {5u, 5u}, request, L"> ***\r\n", Selection{7u, 7u});

    request          = {};
    request.command  = MarkdownCommand::SetList;
    request.listKind = MarkdownListKind::None;
    exact(L"- # Title\n", {4u, 9u}, request, L"# Title\n");
    request.command = MarkdownCommand::OutdentList;
    exact(L"- > quote\n", {4u, 9u}, request, L"> quote\n");
    request         = {};
    request.command = MarkdownCommand::ToggleStyle;
    request.style   = MarkdownTextStyle::Strong;
    unavailable(L"**ab**c", {3u, 7u}, request);
    request.command     = MarkdownCommand::SetLink;
    request.destination = L"https://example.invalid/";
    unavailable(L"**ab**c", {3u, 7u}, request);
    request         = {};
    request.command = MarkdownCommand::InsertParagraph;
    unavailable(L"- > quote", {9u, 9u}, request);
    request         = {};
    request.command = MarkdownCommand::ToggleStyle;
    request.style   = MarkdownTextStyle::Code;
    unavailable(L"**ab**c", {3u, 7u}, request);
    editor->SetText(L"- item\n");
    prepare();
    editor->SetSelection({6u, 6u});
    Check(editor->OnChar(host, L'\r', 0u) && editor->GetText() == L"- item\n- \n", "semantic Enter continues list through ControlHost input");
    Check(editor->Undo(), "keyboard list continuation is undoable");
    prepare();
    editor->SetText(L"hello\n");
    prepare();
    editor->SetSelection({2u, 2u});
    Check(editor->OnChar(host, L'\r', MK_SHIFT) && editor->GetText() == L"he  \nllo\n" && editor->Undo(),
          "Shift+Enter inserts an undoable semantic hard break");
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
    auto tabAppearance            = editor->GetAppearance();
    tabAppearance.sourceTabSpaces = 2u;
    editor->SetAppearance(tabAppearance);
    editor->SetViewMode(ViewMode::Source);
    editor->SetText(L"word");
    editor->SetSelection({4u, 4u});
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText() == L"wo" && editor->Undo() &&
              editor->GetText() == L"word",
          "source backspace run coalesces into one undo");
    editor->SetSelection({0u, 0u});
    Check(editor->OnKeyDown(host, VK_DELETE, 0u) && editor->OnKeyDown(host, VK_DELETE, 0u) && editor->GetText() == L"rd" && editor->Undo() &&
              editor->GetText() == L"word",
          "source forward-delete run coalesces into one undo");
    editor->SetText(L"one\r\ntwo\r\nthree");
    editor->SetSelection({8u, 0u});
    Check(editor->OnKeyDown(host, VK_TAB, 0u) && editor->GetText() == L"  one\r\n  two\r\nthree" && editor->GetSelection() == Selection{12u, 0u},
          "source Tab indents selected physical lines as one backward selection");
    Check(editor->Undo() && editor->GetText() == L"one\r\ntwo\r\nthree", "multiline indentation is one undo transaction");
    editor->SetSelection({0u, 8u});
    Check(editor->OnKeyDown(host, VK_TAB, 0u) && editor->OnKeyDown(host, VK_TAB, MK_SHIFT) && editor->GetText() == L"one\r\ntwo\r\nthree",
          "Shift+Tab removes indentation on the same lines");
    Check(! editor->OnKeyDown(host, VK_TAB, MK_CONTROL), "Ctrl+Tab remains available for host focus traversal");
    tabAppearance.sourceTabUsesTabs = true;
    editor->SetAppearance(tabAppearance);
    editor->SetSelection({0u, 0u});
    Check(editor->OnKeyDown(host, VK_TAB, 0u) && editor->GetText().starts_with(L"\tone"), "source Tab can insert a literal tab");
    tabAppearance.sourceTabUsesTabs = false;
    editor->SetAppearance(tabAppearance);
    editor->SetText(L"alpha beta");
    editor->SetSelection({10u, 10u});
    Check(editor->OnKeyDown(host, VK_BACK, MK_CONTROL) && editor->GetText() == L"alpha " && editor->Undo() && editor->GetText() == L"alpha beta",
          "Ctrl+Backspace deletes a whole source word in one undo transaction");
    editor->SetSelection({0u, 0u});
    Check(editor->OnKeyDown(host, VK_DELETE, MK_CONTROL) && editor->GetText() == L"beta" && editor->Undo(),
          "Ctrl+Delete deletes the next source word in one undo transaction");
    editor->SetViewMode(ViewMode::Rich);
    editor->SetText(L"**hello** world");
    prepare();
    editor->SetSelection({15u, 15u});
    Check(editor->OnKeyDown(host, VK_BACK, MK_CONTROL) && editor->GetText() == L"**hello** " && editor->Undo(),
          "rich Ctrl+Backspace deletes the mapped word while preserving unrelated markup");
    editor->SetText(L"```\nabc\n```\n");
    prepare();
    editor->SetSelection({5u, 5u});
    Check(editor->OnKeyDown(host, VK_TAB, 0u) && editor->GetText() == L"```\na  bc\n```\n", "code Tab inserts configured spaces");
    tabAppearance.sourceTabSpaces = 0u;
    editor->SetAppearance(tabAppearance);
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
    editor->SetText(L"a | b | c\n- | - | -\nx\n");
    prepare();
    const auto pairSyntax = editor->GetMarkdownSyntax();
    std::vector<std::size_t> pairCells(0u);
    for (std::size_t i = 0u; i < pairSyntax->Nodes().size(); ++i)
        if (pairSyntax->Nodes()[i].block.kind == MarkdownBlockKind::TableCell)
            pairCells.push_back(i);
    editor->SetSelection({editor->GetText().size() - 1u, editor->GetText().size() - 1u});
    request.targetCell = pairCells[3u];
    Check(editor->ExecuteMarkdownCommand(request) == MarkdownCommandStatus::Unchanged && editor->OnKeyDown(host, VK_TAB, 0u),
          "pairing fixture reaches the second padded table cell");
    editor->SetAutoPairEnabled(true);
    const std::wstring pairedTable(L"a | b | c\n- | - | -\nx |  | ()\n");
    const auto pairedCaret = pairedTable.find(L"()") + 1u;
    Check(editor->OnChar(host, L'(', 0u) && editor->GetText() == pairedTable && editor->GetSelection() == Selection{pairedCaret, pairedCaret} &&
              editor->Undo() && editor->GetText() == L"a | b | c\n- | - | -\nx\n",
          "auto-pair materializes only the focused padded cell with an interior caret and one undo");
    editor->SetAutoPairEnabled(false);
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
    auto findOwner   = std::make_unique<RichTextEditor>(L"find");
    auto* findEditor = findOwner.get();
    findEditor->SetSearchShortcutsEnabled(true);
    findEditor->SetOnFindRequested([&] { host.SetRoot(std::make_unique<Label>(L"replacement")); });
    host.SetRoot(std::move(findOwner));
    prepare();
    host.SetFocusControl(findEditor, false);
    Check(findEditor->OnKeyDown(host, 'F', MK_CONTROL) && dynamic_cast<Label*>(host.GetRoot()),
          "find request callback can retire its editor without a stale continuation");
    prepare();
}
template <typename Prepare> void MathAndBroaderDiagrams(ControlHost& host, Prepare&& prepare)
{
    for (const std::wstring source : {L"Before $x_i^2+\\frac{1}{2}$ after.\r\n\r\n$$\\begin{bmatrix}a&b\\\\c&d\\end{bmatrix}$$\r\n",
                                      L"```mermaid\nstateDiagram-v2\n[*] --> Ready\nReady --> Active : Begin\n```\n",
                                      L"```mermaid\nclassDiagram\nAnimal <|-- Bird\nBird : +fly()\n```\n"})
    {
        auto root    = std::make_unique<RichTextEditor>(source);
        auto* editor = root.get();
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        host.SetRoot(std::move(root));
        prepare();
        const auto semantic = editor->GetAcknowledgedSemanticDocument(host);
        Check(semantic && editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic,
              "native and embedded owners prepare mathematical and broader diagram objects");
        const auto object = std::ranges::find_if(
            semantic->Elements(), [](const auto& element) { return element.role == SemanticRole::Math || element.role == SemanticRole::Diagram; });
        Check(object != semantic->Elements().end(), "prepared object exists");
        const auto index  = static_cast<std::size_t>(object - semantic->Elements().begin());
        const auto bounds = editor->GetSemanticElementBounds(host, index);
        Check(bounds && bounds->right > bounds->left && bounds->bottom - bounds->top > 20, "prepared native object reserves actual in-document geometry");
        editor->SetSelection({object->source.first, object->source.last});
        const auto selection = editor->GetSelection();
        editor->SetViewMode(ViewMode::Source);
        prepare();
        Check(editor->GetText() == source && editor->GetSelection() == selection && ! editor->CanUndo(),
              "source switch preserves original spelling, selection and history");
        editor->SetViewMode(ViewMode::Rich);
        prepare();
        Check(editor->ReplaceSelection(L"replacement") && editor->Undo() && editor->GetText() == source,
              "atomic source object replacement and undo preserve exact source");
        prepare();
    }
    {
        const std::wstring source = L"Before *mixed* $\\frac{1}{2}$ after\n\n# Heading **bold** $\\frac{1}{2}$ end";
        auto root                 = std::make_unique<RichTextEditor>(source);
        auto* editor              = root.get();
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        host.SetRoot(std::move(root));
        prepare();
        const auto semantic = editor->GetAcknowledgedSemanticDocument(host);
        Check(semantic != nullptr, "math geometry is acknowledged");
        std::array<std::size_t, 2u> maths{};
        std::size_t count{};
        for (std::size_t i = 0; i < semantic->Elements().size(); ++i)
            if (semantic->Elements()[i].role == SemanticRole::Math && count < maths.size())
                maths[count++] = i;
        Check(count == maths.size(), "body and heading each retain a native equation");
        const auto body    = editor->GetSemanticElementBounds(host, maths[0]);
        const auto heading = editor->GetSemanticElementBounds(host, maths[1]);
        Check(body && heading && heading->bottom - heading->top > (body->bottom - body->top) * 1.4f,
              "a heading equation follows the surrounding heading font size");
        const auto adjacent = editor->TryGetTextInputCaretRect(host, 3u);
        const auto edge     = editor->TryGetTextInputCaretRect(host, semantic->Elements()[maths[0]].source.first);
        Check(adjacent && edge && std::abs(adjacent->top - edge->top) < 0.1f && std::abs(adjacent->bottom - edge->bottom) < 0.1f,
              "the caret beside a tall equation stays aligned to its text line");
    }
}
template <typename Prepare> void MathRunFontSize(ControlHost& host, Prepare&& prepare)
{
    {
        const std::wstring source = L"Body $x$ and $\\frac{1}{2}$\n\n<code>$x$</code>\n\n## <code>$x$</code>";
        auto root                 = std::make_unique<RichTextEditor>(source);
        auto* editor              = root.get();
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        auto appearance                   = editor->GetAppearance();
        appearance.bodyFontSizeDip        = 16.f;
        appearance.codeFontSizeDip        = 32.f;
        appearance.headingFontSizeDip[1u] = 48.f;
        editor->SetAppearance(appearance);
        host.SetRoot(std::move(root));
        prepare();
        const auto semantic = editor->GetAcknowledgedSemanticDocument(host);
        Check(semantic != nullptr, "mixed-style math geometry is acknowledged");
        std::array<std::size_t, 4u> maths{};
        std::size_t count{};
        for (std::size_t index = 0u; index < semantic->Elements().size(); ++index)
            if (semantic->Elements()[index].role == SemanticRole::Math && count < maths.size())
                maths[count++] = index;
        Check(count == maths.size(), "body, tall, code and code-heading formulas are independently prepared");
        const auto body    = editor->GetSemanticElementBounds(host, maths[0]);
        const auto code    = editor->GetSemanticElementBounds(host, maths[2]);
        const auto heading = editor->GetSemanticElementBounds(host, maths[3]);
        Check(body && code && heading, "each styled formula has measured owner geometry");
        const auto bodyHeight    = body->bottom - body->top;
        const auto codeHeight    = code->bottom - code->top;
        const auto headingHeight = heading->bottom - heading->top;
        Check(codeHeight > bodyHeight * 1.8f && codeHeight < bodyHeight * 2.2f && headingHeight > codeHeight * 1.35f && headingHeight < codeHeight * 1.65f,
              "math uses the enclosing body, code and explicit heading sizes before the shared math scale");
        const auto adjacent = editor->TryGetTextInputCaretRect(host, 1u);
        const auto edge     = editor->TryGetTextInputCaretRect(host, semantic->Elements()[maths[1]].source.first);
        Check(adjacent && edge && std::abs(adjacent->top - edge->top) < 0.1f && std::abs(adjacent->bottom - edge->bottom) < 0.1f,
              "caret beside a tall formula follows its text line at the current DPI");
    }
}
template <typename Prepare> void InlineMathSourceEditing(ControlHost& host, Prepare&& prepare)
{
    const std::wstring original = L"Before **bold $x$ end** after";
    auto root                   = std::make_unique<RichTextEditor>(original);
    auto* editor                = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    host.SetRoot(std::move(root));
    prepare();
    const auto first = original.find(L"$x$");
    editor->SetSelection({first + 3u, first + 3u});
    Check(editor->OnKeyDown(host, VK_F2, 0u), "F2 enters inline math from its trailing source edge");
    Check(editor->OnKeyDown(host, VK_F2, 0u), "F2 exits before a preparation turn");
    prepare();
    editor->SetSelection({first, first});
    Check(editor->OnKeyDown(host, VK_F2, 0u), "F2 enters exact inline math source");
    prepare();
    auto region = editor->GetSemanticSourceRegion();
    Check(region && region->kind == MarkdownSourceRegionKind::Inline && region->source == MarkdownRange{first, first + 3u},
          "the session owns the inline formula only");
    auto semantic = editor->GetAcknowledgedSemanticDocument(host);
    Check(semantic && semantic->Map().Display().find(L"bold $x$ end") != std::wstring_view::npos,
          "source presentation preserves neighboring formatted paragraph content");
    Check(std::ranges::any_of(semantic->Runs(), [](const auto& run) { return (run.flags & Bold) != 0u; }), "the surrounding emphasis remains a formatted run");
    editor->SetSelection({region->source.first, region->source.last});
    Check(editor->ReplaceSelection(L"$\\bad\n\nunfinished"), "inline source accepts malformed multiline text");
    prepare();
    region   = editor->GetSemanticSourceRegion();
    semantic = editor->GetAcknowledgedSemanticDocument(host);
    Check(region && semantic && semantic->Map().Display().find(L"bold $\\bad\n\nunfinished end") != std::wstring_view::npos,
          "invalid multiline source stays inline with both neighbors");
    Check(editor->Undo() && editor->GetText() == original, "inline edit remains one reversible source change");
    prepare();
    Check(editor->GetSemanticSourceRegion() && editor->OnKeyDown(host, VK_ESCAPE, 0u), "Escape leaves the inline source session");
    prepare();
    Check(! editor->GetSemanticSourceRegion() && editor->GetText() == original, "leaving restores math without changing source");
    editor->SetText(L"**$x$**");
    prepare();
    editor->SetSelection({2u, 2u});
    Check(editor->OnKeyDown(host, VK_F2, 0u), "math within emphasis enters exact source editing");
    prepare();
    editor->SetSelection({2u, 5u});
    Check(editor->ReplaceSelection(L""), "inline math may be deleted between emphasis delimiters");
    prepare();
    semantic = editor->GetAcknowledgedSemanticDocument(host);
    Check(semantic && std::ranges::any_of(semantic->Elements(),
                                          [&](const auto& item) { return item.role == SemanticRole::Math && item.source == MarkdownRange{2u, 2u}; }),
          "empty math insertion remains an inline owner between paired delimiters");
    Check(editor->Undo(), "undo restores math between delimiters");
    prepare();
    Check(editor->OnKeyDown(host, VK_ESCAPE, 0u), "leave restored inline session");
    prepare();
    editor->SetText(L"$x$");
    prepare();
    editor->SetSelection({0u, 0u});
    Check(editor->OnKeyDown(host, VK_F2, 0u), "standalone inline math enters source editing");
    prepare();
    editor->SetSelection({0u, 3u});
    Check(editor->ReplaceSelection(L""), "the entire inline source may be deleted");
    prepare();
    Check(editor->GetSemanticSourceRegion() && editor->GetSemanticSourceRegion()->source == MarkdownRange{0u, 0u} &&
              editor->GetAcknowledgedSemanticDocument(host),
          "empty source retains the session and prepared caret owner");
    Check(editor->Undo(), "undo restores a deleted formula during its source session");
    prepare();
    Check(editor->GetText() == L"$x$" && editor->OnKeyDown(host, VK_F2, 0u), "restored formula leaves its session normally");
}
template <typename Prepare> void ScrollbarAndNotifications(ControlHost& host, Prepare&& prepare)
{
    std::wstring source(0u, L' ');
    for (unsigned line = 0u; line < 300u; ++line)
        source += L"A synthetic document line.\n\n";
    auto root                        = std::make_unique<RichTextEditor>(source);
    auto* editor                     = root.get();
    auto appearance                  = editor->GetAppearance();
    appearance.showVerticalScrollbar = true;
    editor->SetAppearance(appearance);
    editor->SetReadOnly(true);
    host.SetRoot(std::move(root));
    Check(! editor->GetScrollInfo() && ! editor->SetScrollOffsetDip(20.0f), "pending scroll API refuses without advancing preparation");
    prepare();
    const auto info = editor->GetScrollInfo();
    Check(info && info->viewportHeightDip > 0.0f && info->contentHeightDip > info->viewportHeightDip && info->offsetDip == 0.0f,
          "prepared scroll API exposes bounded document and viewport geometry");
    const auto revision  = editor->Revision();
    const auto selection = editor->GetSelection();
    const auto focus     = host.GetFocusControl();
    size_t events        = 0u;
    float published      = -1.0f;
    editor->SetOnScrollChanged([&](float offset)
    {
        ++events;
        published = offset;
    });
    Check(editor->SetScrollOffsetDip(48.0f) && events == 1u && published == 48.0f && host.GetFocusControl() == focus && editor->GetText() == source &&
              editor->GetSelection() == selection && editor->Revision() == revision && ! editor->CanUndo(),
          "read-only programmatic scroll publishes once without source, history, selection or focus changes");
    Check(editor->SetScrollOffsetDip(48.0f) && events == 1u && ! editor->SetScrollOffsetDip(std::numeric_limits<float>::quiet_NaN()),
          "no-op scroll is silent and non-finite requests are rejected");
    Check(editor->SetScrollOffsetDip(1.0e9f) && events == 2u && std::abs(published - (info->contentHeightDip - info->viewportHeightDip)) < .1f &&
              ! editor->OnMouseWheel(host, {}, -WHEEL_DELTA, 0u),
          "scroll clamps to the real extent and wheel input bubbles at the bottom edge");
    Check(editor->SetScrollOffsetDip(0.0f), "scroll fixture returns to the beginning");
    const auto viewport = editor->TryGetTextInputViewportRect();
    Check(viewport.has_value(), "scroll fixture has a text viewport");
    const auto bounds = editor->GetBounds();
    auto point        = D2D1::Point2F(bounds.right - 14.0f, viewport->top + 2.0f);
    Check(editor->OnMouseDown(host, point, false, 0u) && host.GetCapturedControl() == editor, "visible scrollbar thumb captures its own gesture");
    point.y += 80.0f;
    Check(editor->OnMouseMove(host, point, 0u) && editor->GetScrollInfo()->offsetDip > 0.0f && editor->GetSelection() == selection,
          "thumb drag scrolls without selecting source");
    Check(editor->OnMouseUp(host, point, false, 0u) && host.GetCapturedControl() == nullptr, "thumb release ends only its capture");
    Check(editor->SetScrollOffsetDip(0.0f), "scroll fixture resets after drag");
    editor->SetSelection({source.size(), source.size()});
    const auto beforeQuery = editor->GetScrollInfo()->offsetDip;
    static_cast<void>(editor->TryGetTextInputCaretRect(host, source.size()));
    static_cast<void>(editor->GetVisibleSourceRange(host));
    Check(editor->GetScrollInfo()->offsetDip == beforeQuery, "geometry queries never reveal the caret or mutate scroll");
    prepare();
    Check(editor->GetScrollInfo()->offsetDip > 0.0f, "owner preparation reveals the changed selection");
    editor->SetFlowDirection(FlowDirection::RightToLeft);
    prepare();
    Check(editor->SetScrollOffsetDip(0.0f), "RTL fixture begins at the top");
    const auto rtlViewport = editor->TryGetTextInputViewportRect();
    point                  = D2D1::Point2F(editor->GetBounds().left + 14.0f, rtlViewport->top + 2.0f);
    Check(editor->OnMouseDown(host, point, false, 0u) && host.GetCapturedControl() == editor && editor->OnKeyDown(host, VK_ESCAPE, 0u) &&
              host.GetCapturedControl() == nullptr,
          "RTL scrollbar reserves the leading lane and Escape cancels its gesture");
    events = 0u;
    editor->SetOnScrollChanged([&](float offset)
    {
        ++events;
        if (offset > 0.0f)
            Check(editor->SetScrollOffsetDip(0.0f), "nested scroll can return to the prior offset");
    });
    Check(editor->SetScrollOffsetDip(24.0f) && events == 2u && editor->GetScrollInfo()->offsetDip == 0.0f,
          "reentrant scroll observers publish each committed predecessor exactly once");
    editor->SetOnScrollChanged([&](float) { host.SetRoot(std::make_unique<Label>(L"Replacement document")); });
    Check(editor->SetScrollOffsetDip(12.0f) && dynamic_cast<Label*>(host.GetRoot()), "scroll observer may retire its editor safely");
    prepare();
}
template <typename Prepare> void ViewPreferences(ControlHost& host, Prepare&& prepare)
{
    auto root    = std::make_unique<RichTextEditor>(L"iiWW\tT\n");
    auto* editor = root.get();
    editor->SetViewMode(ViewMode::Source);
    auto appearance            = editor->GetAppearance();
    appearance.sourceMonospace = true;
    appearance.bodyFontFamily  = L"Segoe UI";
    appearance.codeFontFamily  = L"Consolas";
    appearance.tabStopDip      = 64.0f;
    editor->SetAppearance(appearance);
    host.SetRoot(std::move(root));
    prepare();
    const auto first    = editor->TryGetTextInputRangeRects(host, 0u, 1u);
    const auto wide     = editor->TryGetTextInputRangeRects(host, 2u, 3u);
    const auto afterTab = editor->TryGetTextInputRangeRects(host, 5u, 6u);
    Check(first && wide && afterTab && ! first->empty() && ! wide->empty() && ! afterTab->empty(), "source font fixture has prepared character rectangles");
    const float normalWidth = first->front().right - first->front().left;
    Check(std::abs(normalWidth - (wide->front().right - wide->front().left)) < .1f && std::abs(afterTab->front().left - first->front().left - 64.0f) < .2f,
          "opt-in source monospace uses the code family and explicit tab stop");
    const auto source    = editor->Snapshot();
    const auto selection = editor->GetSelection();
    const auto revision  = editor->Revision();
    const auto focus     = host.GetFocusControl();
    editor->SetReadOnly(true);
    editor->SetZoom(2.0f);
    Check(editor->GetZoom() == 2.0f && editor->Snapshot().IsSameRevision(source) && editor->Revision() == revision && editor->GetSelection() == selection &&
              ! editor->CanUndo() && host.GetFocusControl() == focus,
          "read-only zoom changes presentation alone");
    prepare();
    const auto enlarged = editor->TryGetTextInputRangeRects(host, 0u, 1u);
    Check(enlarged && ! enlarged->empty() && std::abs((enlarged->front().right - enlarged->front().left) / normalWidth - 2.0f) < .05f,
          "zoom scales actual prepared glyph geometry");
    Check(editor->OnKeyDown(host, L'0', MK_CONTROL) && editor->GetZoom() == 1.0f &&
              editor->OnMouseWheel(host, {}, static_cast<float>(WHEEL_DELTA), MK_CONTROL) && editor->GetZoom() > 1.0f,
          "Control-zero resets zoom and Control-wheel scales fonts");
    RichSemanticTests::Reject<std::invalid_argument>([&] { editor->SetZoom(std::numeric_limits<float>::quiet_NaN()); }, "non-finite zoom rejects atomically");
    Check(editor->GetZoom() > 1.0f && editor->Snapshot().IsSameRevision(source), "rejected zoom preserves prior presentation and source");
    prepare();
}
template <typename Prepare> void HighlightCoverage(ControlHost& host, Prepare&& prepare)
{
    std::wstring source(0u, L' ');
    for (unsigned line = 0u; line < 40u; ++line)
        source += L"Highlight fixture line.\n";
    auto root    = std::make_unique<RichTextEditor>(source);
    auto* editor = root.get();
    editor->SetViewMode(ViewMode::Source);
    host.SetRoot(std::move(root));
    prepare();
    std::vector<RichTextSourceHighlight> ranges(4096u, RichTextSourceHighlight{0u, source.size(), false});
    ranges.back() = {30u, 3u, true};
    editor->SetSourceHighlights(ranges);
    prepare();
    const auto count = editor->DebugGetState().preparedHighlightRectangles;
    Check(editor->GetSourceHighlights().size() == ranges.size() && count >= 40u && count <= 128u,
          "overlapping highlight requests map normalized source coverage rather than multiplying whole-document geometry");
    const auto revision = editor->Revision();
    Check(editor->OnMouseWheel(host, {}, -WHEEL_DELTA, 0u), "normalized highlight viewport scrolls");
    prepare();
    Check(editor->DebugGetState().preparedHighlightRectangles == count && editor->GetText() == source && editor->Revision() == revision && ! editor->CanUndo(),
          "highlight viewport indexing reuses prepared coverage without source/history changes");
    editor->SetText(L"&fjlig;\n");
    editor->SetViewMode(ViewMode::Rich);
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    prepare();
    const std::array atomic{RichTextSourceHighlight{0u, 7u, false}, RichTextSourceHighlight{3u, 1u, true}};
    editor->SetSourceHighlights(atomic);
    prepare();
    Check(editor->DebugGetState().preparedHighlightRectangles == 1u,
          "active coverage inside a transformed source token wins once for the complete atomic glyph range");
}
template <typename Prepare> void Rendering(ControlHost& host, Prepare&& prepare)
{
    ScrollbarAndNotifications(host, prepare);
    ViewPreferences(host, prepare);
    HighlightCoverage(host, prepare);
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
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
struct SourceCaretEditorFixture final : RichTextEditor
{
    using RichTextEditor::ExportTextInputState;
    using RichTextEditor::ImportTextInputState;
    using RichTextEditor::RichTextEditor;
};
#pragma warning(pop)
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
template <typename Prepare> void SpacingAppearance(ControlHost& host, Prepare&& prepare)
{
    const std::wstring source = L"- one\n- two\n\na\n\nb";
    auto root                 = std::make_unique<SourceCaretEditorFixture>(source);
    auto* editor              = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    auto appearance                 = editor->GetAppearance();
    appearance.bodyFontSizeDip      = 16.0f;
    appearance.lineHeightMultiplier = 1.65f;
    editor->SetAppearance(appearance);
    host.SetRoot(std::move(root));
    prepare();
    editor->SetSelection({source.size(), source.size()});
    Check(editor->OnChar(host, L'!', 0u) && editor->Undo(), "spacing fixture retains a real redo transaction");
    editor->SetSelection({2u, 5u});
    prepare();
    const auto snapshot   = editor->Snapshot();
    const auto revision   = editor->Revision();
    const auto syntax     = editor->GetMarkdownSyntax();
    const auto selection  = editor->GetSelection();
    const auto generation = editor->DebugGetState().layoutGeneration;
    const auto one = editor->TryGetTextInputCaretRect(host, 2u), two = editor->TryGetTextInputCaretRect(host, 8u);
    const float bodyLine = 16.0f * 1.65f;
    // Equal-font caret tops share the same native inset. Their advance includes
    // one explicitly configured body line; caret height itself need not equal it.
    Check(one && two && std::abs(two->top - one->top - bodyLine - 8.0f) < .05f, "both hosts start with the actual legacy list gap");
    appearance.paragraphSpacingAfterLines = 1.0f;
    appearance.listItemSpacingDip         = 10.0f;
    editor->SetAppearance(appearance);
    prepare();
    const auto changedOne = editor->TryGetTextInputCaretRect(host, 2u), changedTwo = editor->TryGetTextInputCaretRect(host, 8u);
    Check(changedOne && changedTwo && std::abs(changedTwo->top - changedOne->top - bodyLine - 10.0f) < .05f &&
              editor->DebugGetState().layoutGeneration != generation && editor->Snapshot().IsSameRevision(snapshot) && editor->Revision() == revision &&
              editor->GetMarkdownSyntax() == syntax && editor->GetSelection() == selection && ! editor->CanUndo() && editor->CanRedo(),
          "native/embedded spacing reflow changes actual geometry without source, selection, syntax or history mutation");
    const auto configuredGeneration = editor->DebugGetState().layoutGeneration;
    editor->SetAppearance(appearance);
    Check(editor->DebugGetState().layoutReady && editor->DebugGetState().layoutGeneration == configuredGeneration,
          "idempotent appearance spacing retains prepared geometry");
    for (const float invalid : {-1.0f, 257.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
    {
        auto rejected               = appearance;
        rejected.listItemSpacingDip = invalid;
        RichSemanticTests::Reject<std::invalid_argument>([&] { editor->SetAppearance(rejected); }, "invalid editor list spacing rejects atomically");
        rejected                            = appearance;
        rejected.paragraphSpacingAfterLines = invalid == 257.0f ? 4.01f : invalid;
        RichSemanticTests::Reject<std::invalid_argument>([&] { editor->SetAppearance(rejected); }, "invalid editor paragraph spacing rejects atomically");
        Check(editor->GetAppearance() == appearance && editor->DebugGetState().layoutGeneration == configuredGeneration &&
                  editor->Snapshot().IsSameRevision(snapshot) && editor->GetSelection() == selection && editor->CanRedo(),
              "rejected spacing preserves current appearance, prepared geometry and exact editor state");
    }
    editor->SetViewMode(ViewMode::Source);
    prepare();
    const auto raw                        = editor->TryGetTextInputCaretRect(host, source.size() - 1u);
    appearance.paragraphSpacingAfterLines = 3.0f;
    appearance.listItemSpacingDip         = 100.0f;
    editor->SetAppearance(appearance);
    prepare();
    const auto rawAfter = editor->TryGetTextInputCaretRect(host, source.size() - 1u);
    Check(raw && rawAfter && raw->top == rawAfter->top && raw->bottom == rawAfter->bottom && editor->Snapshot().IsSameRevision(snapshot) &&
              editor->GetSelection() == selection && ! editor->CanUndo() && editor->CanRedo(),
          "semantic spacing is inert in exact Source geometry and history");
    Check(editor->Redo() && editor->GetText() == source + L"!" && editor->Undo() && editor->GetText() == source,
          "spacing and view changes preserve the original one-undo source transaction");
    prepare();
}
template <typename Prepare> void SpacingNavigation(ControlHost& host, Prepare&& prepare)
{
    auto root    = std::make_unique<SourceCaretEditorFixture>(L"");
    auto* editor = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    auto appearance                       = editor->GetAppearance();
    appearance.bodyFontSizeDip            = 16.0f;
    appearance.lineHeightMultiplier       = 1.65f;
    appearance.paragraphSpacingAfterLines = 4.0f;
    appearance.listItemSpacingDip         = 256.0f;
    editor->SetAppearance(appearance);
    host.SetRoot(std::move(root));
    for (const auto flow : {FlowDirection::LeftToRight, FlowDirection::RightToLeft})
    {
        const std::wstring source = L"- iiiii\n- iiiii";
        editor->SetText(source);
        editor->SetFlowDirection(flow);
        prepare();
        const auto snapshot = editor->Snapshot();
        const auto revision = editor->Revision();
        editor->SetSelection({4u, 4u});
        Check(editor->OnKeyDown(host, VK_DOWN, 0u) && editor->GetSelection() == Selection{12u, 12u},
              "native/embedded Down crosses a 256-DIP list gap with preferred x in either flow");
        prepare();
        Check(editor->OnKeyDown(host, VK_UP, 0u) && editor->GetSelection() == Selection{4u, 4u},
              "native/embedded Up reverses the large list gap at the same exact source column");
        prepare();
        Check(editor->OnKeyDown(host, VK_DOWN, MK_SHIFT) && editor->GetSelection() == Selection{4u, 12u},
              "large-gap Shift+Down extends the exact source selection");
        prepare();
        Check(editor->OnKeyDown(host, VK_UP, MK_SHIFT) && editor->GetSelection() == Selection{4u, 4u}, "large-gap Shift+Up preserves the original anchor");
        prepare();
        Check(editor->OnKeyDown(host, VK_UP, 0u) && editor->GetSelection() == Selection{4u, 4u}, "Up at the first prepared visual line stays in place");
        editor->SetSelection({12u, 12u});
        Check(editor->OnKeyDown(host, VK_DOWN, 0u) && editor->GetSelection() == Selection{12u, 12u}, "Down at the final prepared visual line stays in place");
        Check(editor->Snapshot().IsSameRevision(snapshot) && editor->Revision() == revision && editor->GetText() == source && ! editor->CanUndo() &&
                  ! editor->CanRedo(),
              "large-gap navigation changes no source revision or history");
    }
    for (const auto source : {L"a\n# b", L"a\n\nb"})
    {
        editor->SetText(source);
        editor->SetFlowDirection(FlowDirection::LeftToRight);
        prepare();
        editor->SetSelection({0u, 0u});
        const auto firstTarget = std::wstring_view(source) == L"a\n# b" ? 4u : 2u;
        Check(editor->OnKeyDown(host, VK_DOWN, 0u) && editor->GetSelection() == Selection{firstTarget, firstTarget},
              "Down crosses a four-body-line paragraph margin or first visits its real separator site");
        prepare();
        if (firstTarget == 2u)
        {
            Check(editor->OnKeyDown(host, VK_DOWN, 0u) && editor->GetSelection() == Selection{3u, 3u},
                  "Down crosses the remaining margin after its credited source blank");
            prepare();
            Check(editor->OnKeyDown(host, VK_UP, 0u) && editor->GetSelection() == Selection{2u, 2u},
                  "Up returns to the same real blank ACP across a large margin");
            prepare();
        }
        Check(editor->OnKeyDown(host, VK_UP, 0u) && editor->GetSelection() == Selection{0u, 0u} && editor->GetText() == source && ! editor->CanUndo(),
              "paragraph margin navigation reverses without virtual source positions or history");
    }
    prepare();
}
template <typename Prepare> void SourceCaretSitesPolicy(ControlHost& host, Prepare&& prepare, bool spacing)
{
    const std::wstring source = L"\r\n>\r\n> >\n- \r\n\r\n";
    auto root                 = std::make_unique<SourceCaretEditorFixture>(source);
    auto* editor              = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    if (spacing)
    {
        auto appearance                       = editor->GetAppearance();
        appearance.paragraphSpacingAfterLines = 1.0f;
        appearance.listItemSpacingDip         = 10.0f;
        editor->SetAppearance(appearance);
    }
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
template <typename Prepare> void SourceCaretSites(ControlHost& host, Prepare&& prepare)
{
    SpacingAppearance(host, prepare);
    SpacingNavigation(host, prepare);
    SourceCaretSitesPolicy(host, prepare, false);
    SourceCaretSitesPolicy(host, prepare, true);
}
template <typename Prepare, typename PrepareDpi> void RenderingAdmissionFallback(ControlHost& host, Prepare&& prepare, PrepareDpi&& prepareDpi)
{
#if ! DXUI_ENABLE_DIAGNOSTICS
    static_cast<void>(prepareDpi);
#endif
#if DXUI_ENABLE_DIAGNOSTICS
    const auto previousFailure  = RichText::Detail::testRichPreparationFailure;
    const auto previousAttempts = RichText::Detail::testRichPreparationFailureAttempts;
    const auto restoreFailure   = wil::scope_exit([&]() noexcept
    {
        RichText::Detail::testRichPreparationFailure         = previousFailure;
        RichText::Detail::testRichPreparationFailureAttempts = previousAttempts;
    });
    for (const unsigned stage : {1u, 2u})
    {
        RichText::Detail::testRichPreparationFailure         = stage;
        RichText::Detail::testRichPreparationFailureAttempts = 0u;
        const std::wstring original                          = L"**retained** and $x$\n";
        auto root                                            = std::make_unique<RichTextEditor>(original);
        auto* editor                                         = root.get();
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        editor->SetSelection({2u, 5u});
        host.SetRoot(std::move(root));
        prepare();
        Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::SourceFallback && editor->DebugGetState().layoutReady &&
                  ! editor->GetSemanticDocument() && editor->GetText() == original && editor->GetSelection() == Selection{2u, 5u} && ! editor->CanUndo() &&
                  editor->GetViewMode() == ViewMode::Rich && RichText::Detail::testRichPreparationFailureAttempts == 1u,
              "deterministic rich failure falls back once without changing source, selection, history or requested mode");
        prepare();
        Check(RichText::Detail::testRichPreparationFailureAttempts == 1u, "unchanged rich failure key does not retry on idle");
        prepareDpi(144u);
        Check(host.GetDpi() == 144.f && RichText::Detail::testRichPreparationFailureAttempts == 2u &&
                  editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::SourceFallback,
              "changed DPI retries the deterministic preparation key");
        prepareDpi(96u);
        Check(host.GetDpi() == 96.f && RichText::Detail::testRichPreparationFailureAttempts == 3u, "restored DPI is a distinct preparation request");
        auto appearance = editor->GetAppearance();
        appearance.zoom = 1.25f;
        editor->SetAppearance(appearance);
        prepare();
        Check(RichText::Detail::testRichPreparationFailureAttempts == 4u && editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::SourceFallback,
              "changed appearance retries a deterministic preparation key");
        RichText::Detail::testRichPreparationFailure = 0u;
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
        prepare();
        Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && editor->GetText() == original &&
                  editor->GetSelection() == Selection{2u, 5u} && ! editor->CanUndo(),
              "changed profile recovers semantic presentation with source/history intact");
        RichText::Detail::testRichPreparationFailure = stage;
        editor->SetText(L"new **source**\n");
        prepare();
        Check(RichText::Detail::testRichPreparationFailureAttempts == 5u && editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::SourceFallback,
              "new source retries the deterministic preparation key");
        RichText::Detail::testRichPreparationFailure = 0u;
        editor->SetText(L"recovered **source**\n");
        prepare();
        Check(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic, "new source recovers after the fault is removed");
    }
#endif
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
inline void NativeMathHRESULTFailureContainment(ControlHost& host);
template <typename Prepare> void NativeObjectFailureContainment(ControlHost& host, Prepare&& prepare)
{
    const std::wstring source = L"# Heading\n\n$y$ and [before $\\unknown{x}$ after](https://example.test \"original title\")\n\n$$\\text{" +
                                std::wstring(1400u, L'm') + L"}$$\n\n**after**\n";
    auto root                 = std::make_unique<RichTextEditor>(source);
    auto* editor              = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    auto appearance            = editor->GetAppearance();
    appearance.bodyFontSizeDip = 96.f;
    editor->SetAppearance(appearance);
    editor->SetSelection({3u, 3u});
    host.SetRoot(std::move(root));
    prepare();
    const auto rejected = editor->GetAcknowledgedSemanticDocument(host);
    Check(rejected && editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && rejected->Diagnostics().size() == 2u &&
              rejected->Map().Display().starts_with(L"Heading\n") && rejected->Map().Display().ends_with(L"after\n"),
          "malformed and over-wide formulas fall back locally while headings and emphasis stay semantic");
    const auto large = std::ranges::find_if(rejected->Elements(),
                                            [](const auto& element) { return element.role == SemanticRole::Math && element.name.starts_with(L"\\text{"); });
    Check(large != rejected->Elements().end() && large->display.last > large->display.first + 1u &&
              rejected->Map().Display().substr(large->display.first, large->display.last - large->display.first).starts_with(L"$$\\text{"),
          "geometry rejection preserves the complete exact equation envelope as visible source");
    const auto heldDisplay = std::wstring(rejected->Map().Display());
    const auto before      = editor->DebugGetState();
    prepare();
    const auto idle = editor->DebugGetState();
    Check(idle.preparationAdvances == before.preparationAdvances && idle.semanticPreparationAdvances == before.semanticPreparationAdvances &&
              editor->GetText() == source && editor->GetSelection() == Selection{3u, 3u} && ! editor->CanUndo(),
          "local rejection neither retries while idle nor changes source, selection or history");
    appearance.bodyFontSizeDip = 12.f;
    editor->SetAppearance(appearance);
    prepare();
    const auto retried = editor->GetAcknowledgedSemanticDocument(host);
    Check(retried && retried != rejected && retried->Diagnostics().size() == 1u && rejected->Map().Display() == heldDisplay &&
              std::ranges::count_if(retried->Elements(),
                                    [](const auto& element)
    { return element.role == SemanticRole::Math && element.display.last == element.display.first + 1u; }) == 2u,
          "font changes retry geometry failures while held publications and syntax rejection stay stable");

    const std::wstring diagramSource = L"# Heading\n\n```mermaid\ngraph LR;A[alpha]-->B[beta]\n```\n\n**after**\n";
    editor->SetText(diagramSource);
    appearance.maximumTextWidthDip = 80.f;
    editor->SetAppearance(appearance);
    prepare();
    const auto narrow = editor->GetAcknowledgedSemanticDocument(host);
    Check(narrow && narrow->Diagnostics().size() == 1u && narrow->Map().Display() == L"Heading\n```mermaid\ngraph LR;A[alpha]-->B[beta]\n```\nafter\n",
          "a diagram width failure keeps the whole fence visible and leaves adjacent rich blocks intact");
    const auto narrowAdvances = editor->DebugGetState().preparationAdvances;
    prepare();
    Check(editor->DebugGetState().preparationAdvances == narrowAdvances, "an unchanged narrow diagram has no geometry retry loop");
    appearance.maximumTextWidthDip = 640.f;
    editor->SetAppearance(appearance);
    prepare();
    const auto wide = editor->GetAcknowledgedSemanticDocument(host);
    Check(wide && wide->Diagnostics().empty() && std::ranges::count(wide->Elements(), SemanticRole::Diagram, &SemanticElement::role) == 1u &&
              editor->GetText() == diagramSource && ! editor->CanUndo(),
          "a wider view retries the original diagram without source edits or hidden stale state");

    for (const std::wstring fence :
         {L"> ```mermaid\r\n> graph LR;A[alpha]-->B[beta]\r\n> ```\r\n", L"- ```mermaid\r\n  graph LR;A[alpha]-->B[beta]\r\n  ```\r\n"})
    {
        const auto prefixedSource = L"# Before\r\n\r\n" + fence + L"\r\n**After**\r\n";
        editor->SetText(prefixedSource);
        appearance.maximumTextWidthDip = 80.f;
        editor->SetAppearance(appearance);
        prepare();
        const auto fallback = editor->GetAcknowledgedSemanticDocument(host);
        Check(fallback && fallback->Diagnostics().size() == 1u, "a prefixed CRLF fence has one local geometry diagnostic");
        const auto& diagnostic = fallback->Diagnostics()[0u];
        Check(diagnostic.element < fallback->Elements().size(), "local fence diagnostic retains a live source owner");
        const auto& owner = fallback->Elements()[diagnostic.element];
        Check(owner.role == SemanticRole::Code && diagnostic.source == owner.source &&
                  prefixedSource.substr(owner.source.first, owner.source.last - owner.source.first) == fence &&
                  fallback->Map().Display().substr(owner.display.first, owner.display.last - owner.display.first) == fence &&
                  fallback->Map().Display().ends_with(L"After\n") && editor->GetText() == prefixedSource && ! editor->CanUndo(),
              "local fallback preserves every raw quote/list prefix and CRLF while adjacent emphasis remains rich");
        Check(fallback->Map().ToSource(owner.display.first, MappingAffinity::Leading) == owner.source.first &&
                  fallback->Map().ToDisplay(owner.source.first, MappingAffinity::Leading) == owner.display.first,
              "literal fence geometry and source mapping share the exact opening ACP");
        const auto settled = editor->DebugGetState();
        prepare();
        Check(editor->DebugGetState().semanticPreparationAdvances == settled.semanticPreparationAdvances &&
                  editor->DebugGetState().preparationAdvances == settled.preparationAdvances,
              "prefixed fence rejection remains idle without rebuilding its semantic publication");
    }
    NativeMathHRESULTFailureContainment(host);
}

inline void NativeMathHRESULTFailureContainment(ControlHost& host)
{
#if DXUI_ENABLE_DIAGNOSTICS
    {
        std::wstring source;
        for (unsigned i = 0u; i < 150u; ++i)
            source += L"A paragraph with **styled** text.\n\n";
        auto root    = std::make_unique<DirectPrepareRichTextEditor>(source);
        auto* editor = root.get();
        editor->SetBounds(D2D1::RectF(0.f, 0.f, 480.f, 240.f));
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        host.SetRoot(std::move(root));
        bool sharedTurn = false;
        for (unsigned turn = 0u; turn < 1024u; ++turn)
        {
            const auto status = editor->PrepareOnce(host);
            const auto state  = editor->DebugGetState();
            Check(state.lastAdvanceMarkdownWork + state.lastAdvanceSemanticWork <= 4096u, "parser and semantic builder share the owner-turn work budget");
            sharedTurn = sharedTurn || (state.lastAdvanceMarkdownWork && state.lastAdvanceSemanticWork);
            if (status != S_FALSE)
            {
                Check(status == S_OK && sharedTurn && editor->GetAcknowledgedSemanticDocument(host),
                      "completed parser starts semantic preparation in the same turn when budget remains");
                break;
            }
            Check(turn != 1023u, "shared-budget preparation settles");
        }
    }
    {
        auto root    = std::make_unique<DirectPrepareRichTextEditor>(L"```mermaid\ngraph LR;A[Draft]-->B[Ready]\n```\n");
        auto* editor = root.get();
        editor->SetBounds(D2D1::RectF(0.f, 0.f, 480.f, 240.f));
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        host.SetRoot(std::move(root));
        SemanticWriteFactoryOverride override(host);
        auto* calls           = override.Get();
        calls->failLayoutText = L"Draft";
        Check(PrepareUntilSettled(*editor, host) == S_OK && calls->matchingLayouts > 0u, "prepare a measured native diagram");
        const auto labels = calls->matchingLayouts;
        editor->SetBounds(D2D1::RectF(0.f, 0.f, 500.f, 240.f));
        Check(PrepareUntilSettled(*editor, host) == S_OK && calls->matchingLayouts == labels && editor->GetAcknowledgedSemanticDocument(host),
              "width-only reflow reuses the immutable prepared diagram without reshaping labels");
    }
    const auto install = [&](std::wstring source)
    {
        auto root    = std::make_unique<DirectPrepareRichTextEditor>(std::move(source));
        auto* editor = root.get();
        editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        editor->SetMarkdownRenderingEnabled(true);
        host.SetRoot(std::move(root));
        return editor;
    };

    {
        const std::wstring source = L"# Heading\n\n[before $x$ after](https://example.test \"original title\") and $y$ **bold suffix**\n";
        auto* editor              = install(source);
        const auto caret          = source.find(L"$x$") + 1u;
        editor->SetSelection({caret, caret});
        SemanticWriteFactoryOverride override(host);
        auto* calls                    = override.Get();
        calls->failLayoutText          = L"x";
        calls->layoutFailure           = DWRITE_E_UNSUPPORTEDOPERATION;
        calls->failMatchingLayoutCount = 1u;
        Check(PrepareUntilSettled(*editor, host) == S_OK, "font-local unsupported math operation reaches complete source fallback");
        const auto fallback = editor->GetAcknowledgedSemanticDocument(host);
        const SemanticElement* failedMath{};
        const SemanticElement* heading{};
        const SemanticElement* link{};
        std::size_t supportedMathCount{};
        if (fallback)
            for (const auto& element : fallback->Elements())
            {
                if (element.role == SemanticRole::Math && element.name == L"x")
                    failedMath = &element;
                if (element.role == SemanticRole::Math && element.display.last == element.display.first + 1u)
                    ++supportedMathCount;
                if (element.role == SemanticRole::Heading && element.heading == 1u)
                    heading = &element;
                if (element.role == SemanticRole::Link)
                    link = &element;
            }
        Check(fallback && fallback->Diagnostics().size() == 1u && failedMath && fallback->Syntax()->Slice(failedMath->source) == L"$x$" &&
                  failedMath->title == L"Equation cannot be prepared natively for this view. Exact source is preserved." && supportedMathCount == 1 &&
                  fallback->Map().Display().find(L"$x$") != std::wstring::npos && fallback->Map().Display().find(L"\ufffc") != std::wstring::npos &&
                  fallback->Map().Display().find(L"Heading") != std::wstring::npos && fallback->Map().Display().find(L"# Heading") == std::wstring::npos &&
                  fallback->Map().Display().find(L"bold suffix") != std::wstring::npos &&
                  fallback->Map().Display().find(L"**bold suffix**") == std::wstring::npos && heading && link &&
                  failedMath->parent == static_cast<std::size_t>(link - fallback->Elements().data()) && link->title == L"original title" &&
                  link->destination == L"https://example.test" && editor->GetText() == source && editor->GetSelection() == Selection{caret, caret} &&
                  ! editor->CanUndo(),
              "font-local rejection preserves the exact formula source, its title, supported math, and neighboring rich semantic structure");
        const auto attempts = calls->matchingLayouts;
        Check(PrepareUntilSettled(*editor, host) == S_OK && calls->matchingLayouts == attempts,
              "the same unsupported math/key is not retried during idle preparation");

        editor->SetSelection({0u, 0u});
        Check(editor->ReplaceSelection(L"prefix\n\n"), "insert unrelated source before rejected formula");
        Check(PrepareUntilSettled(*editor, host) == S_OK && calls->matchingLayouts == attempts &&
                  editor->GetAcknowledgedSemanticDocument(host)->Diagnostics().size() == 1u,
              "a shifted unchanged rejected formula is not retried on an unrelated edit");
        Check(editor->Undo() && PrepareUntilSettled(*editor, host) == S_OK && calls->matchingLayouts == attempts,
              "undo of the unrelated edit retains the rejected formula key");
        editor->SetSelection({caret, caret});
        auto appearance            = editor->GetAppearance();
        appearance.bodyFontSizeDip = 13.f;
        editor->SetAppearance(appearance);
        Check(PrepareUntilSettled(*editor, host) == S_OK, "a changed math geometry key is eligible for native retry");
        const auto recovered                = editor->GetAcknowledgedSemanticDocument(host);
        const auto recoveredObjectMathCount = recovered ? std::ranges::count_if(recovered->Elements(),
                                                                                [](const auto& element)
        { return element.role == SemanticRole::Math && element.display.last == element.display.first + 1u; })
                                                        : 0;
        Check(recovered && recovered->Diagnostics().empty() && recoveredObjectMathCount == 2 && calls->matchingLayouts > attempts &&
                  editor->GetText() == source && editor->GetSelection() == Selection{caret, caret} && ! editor->CanUndo(),
              "changed-key native math succeeds without editing source or adding undo history");
    }

    {
        auto* editor = install(L"$x$\n");
        SemanticWriteFactoryOverride override(host);
        auto* calls                    = override.Get();
        calls->failLayoutText          = L"x";
        calls->layoutFailure           = DWRITE_E_UNSUPPORTEDOPERATION;
        calls->failMatchingLayoutCount = 1u;
        Check(PrepareUntilSettled(*editor, host) == S_OK && editor->GetAcknowledgedSemanticDocument(host)->Diagnostics().size() == 1u,
              "seed font-local formula rejection");
        const auto attempts = calls->matchingLayouts;
        editor->SetSelection({0u, 0u});
        Check(editor->ReplaceSelection(L"# ") && PrepareUntilSettled(*editor, host) == S_OK && calls->matchingLayouts > attempts &&
                  editor->GetAcknowledgedSemanticDocument(host)->Diagnostics().empty(),
              "changing the surrounding paragraph into a heading retries the unchanged formula");
    }
    {
        constexpr std::wstring_view source = L"$x$ and $x$\n";
        auto* editor                       = install(std::wstring(source));
        SemanticWriteFactoryOverride override(host);
        auto* calls                    = override.Get();
        calls->failLayoutText          = L"x";
        calls->layoutFailure           = DWRITE_E_UNSUPPORTEDOPERATION;
        calls->failMatchingLayoutCount = 2u;
        Check(PrepareUntilSettled(*editor, host) == S_OK, "multiple native math failures settle together");
        const auto document = editor->GetAcknowledgedSemanticDocument(host);
        Check(document && document->Diagnostics().size() == 2u && calls->failedLayouts == 2u && editor->DebugGetState().semanticPreparationAdvances == 2u &&
                  editor->GetText() == source,
              "all unsupported formula owners produce one semantic fallback rebuild without partial publication");
    }
    {
        constexpr std::wstring_view source = L"$x$\n";
        auto* editor                       = install(std::wstring(source));
        editor->SetSelection({1u, 1u});
        SemanticWriteFactoryOverride override(host);
        auto* calls                    = override.Get();
        calls->failLayoutText          = L"x";
        calls->layoutFailure           = DWRITE_E_UNEXPECTED;
        calls->failMatchingLayoutCount = 1u;
        const auto failed              = PrepareUntilSettled(*editor, host);
        Check(failed == DWRITE_E_UNEXPECTED && calls->failedLayouts == 1u, "retryable DirectWrite failure preserves its HRESULT at the editor boundary");
        Check(PrepareUntilSettled(*editor, host) == S_OK, "the unchanged math key retries after a transient DirectWrite failure");
        const auto recovered = editor->GetAcknowledgedSemanticDocument(host);
        Check(recovered && recovered->Diagnostics().empty() && calls->matchingLayouts >= 2u && editor->GetText() == source &&
                  editor->GetSelection() == Selection{1u, 1u} && ! editor->CanUndo(),
              "transient DirectWrite failure is not latched as formula source fallback");
    }

    {
        constexpr std::wstring_view source = L"$x$\n";
        auto* editor                       = install(std::wstring(source));
        editor->SetSelection({1u, 1u});
        SemanticWriteFactoryOverride override(host);
        auto* calls                    = override.Get();
        calls->failLayoutText          = L"x";
        calls->layoutFailure           = E_OUTOFMEMORY;
        calls->failMatchingLayoutCount = 1u;
        const auto failed              = PrepareUntilSettled(*editor, host);
        Check(failed == E_OUTOFMEMORY && calls->failedLayouts == 1u,
              "DirectWrite allocation HRESULT remains E_OUTOFMEMORY instead of becoming capacity fallback");
        Check(PrepareUntilSettled(*editor, host) == S_OK, "same-key formula preparation can retry after DirectWrite allocation failure");
        const auto recovered = editor->GetAcknowledgedSemanticDocument(host);
        Check(recovered && recovered->Diagnostics().empty() && editor->GetText() == source && editor->GetSelection() == Selection{1u, 1u} &&
                  ! editor->CanUndo(),
              "allocation failure never publishes a permanent formula rejection");
    }

    {
        constexpr std::wstring_view source = L"$x$\n";
        auto* editor                       = install(std::wstring(source));
        editor->SetSelection({1u, 1u});
        SemanticWriteFactoryOverride override(host);
        auto* calls                           = override.Get();
        calls->failLayoutText                 = L"x";
        const auto previousLineMetricsFailure = RichText::Detail::testRichMathLineMetricsFailure;
        const auto restoreLineMetricsFailure =
            wil::scope_exit([previousLineMetricsFailure]() noexcept { RichText::Detail::testRichMathLineMetricsFailure = previousLineMetricsFailure; });
        RichText::Detail::testRichMathLineMetricsFailure = E_OUTOFMEMORY;
        const auto failed                                = PrepareUntilSettled(*editor, host);
        Check(failed == E_OUTOFMEMORY && calls->failedLayouts == 0u,
              "line-metrics allocation HRESULT is not converted into a permanent one-line capacity rejection");
        Check(PrepareUntilSettled(*editor, host) == S_OK, "line-metrics allocation failure remains retryable for the unchanged key");
        const auto recovered = editor->GetAcknowledgedSemanticDocument(host);
        Check(recovered && recovered->Diagnostics().empty() && editor->GetText() == source && editor->GetSelection() == Selection{1u, 1u} &&
                  ! editor->CanUndo(),
              "line-metrics retry publishes native math rather than source fallback");
        RichText::Detail::testRichMathLineMetricsFailure = S_OK;
    }
    {
        SemanticWriteFactoryOverride override(host);
        auto expression = std::make_shared<const MathExpression>(ParseMath(L"\\Big("));
        RichText::Detail::PreparedMath probe(override.Get(), expression, 24.f);
        while (! probe.Advance())
        {
        }
        if (! probe.VariantLayouts().empty())
        {
            constexpr std::wstring_view source = L"$\\Big($\n";
            auto* editor                       = install(std::wstring(source));
            editor->SetSelection({1u, 1u});
            const auto priorFailure   = RichText::Detail::testRichMathGlyphMetricsFailure;
            const auto restoreFailure = wil::scope_exit([priorFailure]() noexcept { RichText::Detail::testRichMathGlyphMetricsFailure = priorFailure; });
            RichText::Detail::testRichMathGlyphMetricsFailure = E_OUTOFMEMORY;
            const auto failed                                 = PrepareUntilSettled(*editor, host);
            Check(failed == E_OUTOFMEMORY && RichText::Detail::testRichMathGlyphMetricsFailure == S_OK,
                  "glyph-metrics allocation HRESULT stays retryable instead of publishing fallback");
            Check(PrepareUntilSettled(*editor, host) == S_OK, "glyph-metrics allocation failure retries for the unchanged key");
            const auto recovered = editor->GetAcknowledgedSemanticDocument(host);
            Check(recovered && recovered->Diagnostics().empty() && editor->GetText() == source && editor->GetSelection() == Selection{1u, 1u},
                  "glyph-metrics retry publishes native math without changing source or selection");
        }
    }
#else
    static_cast<void>(host);
#endif
}

inline void SourceWindowUnicodeParity()
{
    using namespace DxUi::RichText::Detail;
    std::vector<std::wstring> cases;
    cases.push_back(std::wstring(400u, L'a') + L" \r\nC:\\folder\\file -- next \U0001f600a\u0301 text" + std::wstring(400u, L'z'));
    std::wstring regional(200u, L'x');
    for (unsigned i = 0u; i < 200u; ++i)
        regional += L"\U0001f1e6";
    regional += std::wstring(200u, L'y');
    cases.push_back(regional);
    std::wstring indic(180u, L'x');
    for (unsigned i = 0u; i < 180u; ++i)
        indic += L"\u0915\u094d";
    indic += L"\u0915" + std::wstring(180u, L'y');
    cases.push_back(indic);
    std::wstring emoji(200u, L'x');
    for (unsigned i = 0u; i < 150u; ++i)
        emoji += L"\U0001f469\u200d";
    emoji += L"\U0001f469" + std::wstring(200u, L'y');
    cases.push_back(emoji);
    auto counted = std::wstring(180u, L'x');
    counted.push_back(L'\0');
    counted += L"\r\nword" + std::wstring(180u, L'y');
    cases.push_back(counted);
    for (const auto& text : cases)
    {
        Document document(text);
        const auto source = document.Snapshot();
        for (size_t caret = 0u; caret <= text.size(); caret += 7u)
        {
            if (! source.IsBoundary(caret))
                continue;
            for (const auto operation : {SourceWindowOperation::Previous,
                                         SourceWindowOperation::Next,
                                         SourceWindowOperation::PreviousWord,
                                         SourceWindowOperation::NextWord,
                                         SourceWindowOperation::DeleteBackward,
                                         SourceWindowOperation::DeleteForward})
            {
                TextElementRange expected{};
                switch (operation)
                {
                    case SourceWindowOperation::Previous: expected = {StepToPreviousTextElement(text, caret), caret}; break;
                    case SourceWindowOperation::Next: expected = {caret, StepToNextTextElement(text, caret)}; break;
                    case SourceWindowOperation::PreviousWord: expected = {FindPreviousWordBoundary(text, caret), caret}; break;
                    case SourceWindowOperation::NextWord: expected = {caret, FindNextWordBoundary(text, caret)}; break;
                    case SourceWindowOperation::DeleteBackward: expected = GetTextElementDeletionRange(text, caret, true); break;
                    case SourceWindowOperation::DeleteForward: expected = GetTextElementDeletionRange(text, caret, false); break;
                    case SourceWindowOperation::Home: expected = {0u, 0u}; break;
                    case SourceWindowOperation::End: expected = {text.size(), text.size()}; break;
                }
                const auto actual = SourceWindowRange(source, caret, operation);
                Check(actual.first == expected.first && actual.last == expected.last, "source window preserves full Unicode and word boundary results");
            }
        }
    }
    Document longAscii(std::wstring(65536u, L'a'));
    const auto snapshot = longAscii.Snapshot();
    const auto first    = SourceWindowLeft(snapshot, 32768u - 128u);
    const auto last     = SourceWindowRight(snapshot, 32768u + 128u);
    Check(last - first <= 258u, "ordinary source caret obtains a bounded local context");
    const auto wholeWord = SourceWindowRange(snapshot, 32768u, SourceWindowOperation::PreviousWord);
    Check(wholeWord.first == 0u && wholeWord.last == 32768u, "unbroken long words expand safely to their true boundary");
}
inline void RichSourceWindowParity()
{
    using namespace DxUi::RichText::Detail;
    std::vector<std::wstring> cases = {L"# **heading**\r\nplain *italic* `code`\n## **next**\rfinal\n",
                                       L"first\nC:\\folder\\word\nlast *item*\n",
                                       L"C:\\folder\\word\n# hidden\n",
                                       L"# \n# \r\n# **a**\n# ",
                                       std::wstring(300u, L'a') + L"\n# **hidden** end\r\n" + std::wstring(300u, L'b'),
                                       L"# \U0001f469\u200d\U0001f4bb\n**\u0915\u094d\u0915**\n\U0001f1e6\U0001f1e7\U0001f1e8\n"};
    std::wstring manyLines(0u, L' ');
    for (unsigned line = 0u; line < 45u; ++line)
        manyLines += line == 20u ? L"C:\\folder\\word\r\n" : L"# **hidden** a `code`\r\n";
    cases.push_back(manyLines);
    for (const auto& text : cases)
    {
        Document document(text);
        const auto snapshot = document.Snapshot();
        const auto full     = Project(snapshot, document.Revision(), ViewMode::Rich);
        for (size_t caret = 0u; caret <= text.size(); ++caret)
        {
            if (! snapshot.IsBoundary(caret))
                continue;
            const auto position = full.ToDisplay(caret);
            for (const auto operation : {SourceWindowOperation::Previous,
                                         SourceWindowOperation::Next,
                                         SourceWindowOperation::PreviousWord,
                                         SourceWindowOperation::NextWord,
                                         SourceWindowOperation::Home,
                                         SourceWindowOperation::End})
            {
                size_t display{};
                switch (operation)
                {
                    case SourceWindowOperation::Previous: display = StepToPreviousTextElement(full.text, position); break;
                    case SourceWindowOperation::Next: display = StepToNextTextElement(full.text, position); break;
                    case SourceWindowOperation::PreviousWord: display = FindPreviousWordBoundary(full.text, position); break;
                    case SourceWindowOperation::NextWord: display = FindNextWordBoundary(full.text, position); break;
                    case SourceWindowOperation::Home: display = 0u; break;
                    case SourceWindowOperation::End: display = full.text.size(); break;
                    case SourceWindowOperation::DeleteBackward:
                    case SourceWindowOperation::DeleteForward: break;
                }
                const bool trailing =
                    operation == SourceWindowOperation::Next || operation == SourceWindowOperation::NextWord || operation == SourceWindowOperation::End;
                const auto expected = full.ToSource(display, trailing);
                const auto actual   = RichSourceWindowRange(snapshot, caret, operation);
                Check((trailing ? actual.last : actual.first) == expected, "v0 rich local projection preserves full caret mapping");
            }
            for (const bool backwards : {true, false})
                for (const bool byWord : {true, false})
                {
                    const auto display   = byWord ? (backwards ? TextElementRange{FindPreviousWordBoundary(full.text, position), position}
                                                               : TextElementRange{position, FindNextWordBoundary(full.text, position)})
                                                  : GetTextElementDeletionRange(full.text, position, backwards);
                    const auto operation = byWord ? (backwards ? SourceWindowOperation::PreviousWord : SourceWindowOperation::NextWord)
                                                  : (backwards ? SourceWindowOperation::DeleteBackward : SourceWindowOperation::DeleteForward);
                    const auto actual    = RichSourceWindowRange(snapshot, caret, operation, true);
                    if (display.first == display.last)
                        Check(actual.first == actual.last, "v0 rich empty display deletion is a no-op");
                    else
                        Check(actual.first == full.ToSource(display.first) && actual.last == full.ToSource(display.last, true),
                              "v0 rich local projection preserves full deletion mapping");
                }
        }
    }
}
inline void PendingRichSourceWindowTransport(ControlHost& host)
{
    const std::wstring source = L"# **heading**\r\nnormal *word*\n";
    auto owned                = std::make_unique<RichTextEditor>(source);
    auto* editor              = owned.get();
    editor->SetViewMode(ViewMode::Rich);
    host.SetRoot(std::move(owned));
    host.SetFocusControl(editor, false);
    const auto caret = source.find(L"heading") + 7u;
    editor->SetSelection({caret, caret});
    const auto projection = Project(editor->Snapshot(), editor->Revision(), ViewMode::Rich);
    const auto position   = projection.ToDisplay(caret);
    const auto previous   = projection.ToSource(StepToPreviousTextElement(projection.text, position));
    Check(editor->OnKeyDown(host, VK_LEFT, 0u) && editor->GetSelection() == Selection{previous, previous},
          "pending v0 Rich caret uses full-projection mapping across hidden markup");
    editor->SetSelection({caret, caret});
    const auto deletion = GetTextElementDeletionRange(projection.text, position, true);
    const auto first = projection.ToSource(deletion.first), last = projection.ToSource(deletion.last, true);
    auto expected = source;
    expected.erase(first, last - first);
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText() == expected && editor->Undo() && editor->GetText() == source,
          "pending v0 Rich erase retains hidden-marker source mapping and undo");
}
inline void PendingSourceWindowTransport(ControlHost& host)
{
    const std::wstring source = std::wstring(500u, L'a') + L"\r\n\U0001f469\u200d\U0001f4bb text" + std::wstring(500u, L'z');
    auto owned                = std::make_unique<RichTextEditor>(source);
    auto* editor              = owned.get();
    editor->SetViewMode(ViewMode::Source);
    host.SetRoot(std::move(owned));
    host.SetFocusControl(editor, false);
    editor->SetSelection({500u, 500u});
    Check(editor->OnKeyDown(host, VK_RIGHT, 0u) && editor->GetSelection() == Selection{502u, 502u}, "pending layout moves across CRLF as one element");
    Check(editor->OnKeyDown(host, VK_RIGHT, 0u) && editor->GetSelection() == Selection{507u, 507u}, "pending layout moves across a full emoji ZWJ cluster");
    Check(editor->OnKeyDown(host, VK_BACK, 0u) && editor->GetText() == std::wstring(500u, L'a') + L"\r\n text" + std::wstring(500u, L'z') && editor->Undo() &&
              editor->GetText() == source,
          "pending layout erases one emoji cluster with one undo");
}
template <typename Prepare> void UrlOnSelectionPaste(ControlHost& host, Prepare&& prepare)
{
    auto owned   = std::make_unique<RichTextEditor>(L"hello **world**");
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    auto appearance                = editor->GetAppearance();
    appearance.pasteUrlOnSelection = true;
    editor->SetAppearance(appearance);
    host.SetRoot(std::move(owned));
    host.SetFocusControl(editor);
    prepare();
    editor->SetSelection({0u, 5u});
    TextClipboardContent content;
    content.text = L"https://example.invalid/a";
    std::wstring replacement;
    const bool transformedUrl = editor->TryPrepareClipboardPaste(content, replacement);
    Check(transformedUrl && replacement == L"[hello](<https://example.invalid/a>)", "URL paste prepares a source-aware selected link");
    Check(editor->ReplaceSelection(replacement) && editor->GetText() == L"[hello](<https://example.invalid/a>) **world**" && editor->Undo() &&
              editor->GetText() == L"hello **world**",
          "URL paste is one undo transaction");
    prepare();
    editor->SetSelection({0u, 5u});
    content.text = L"HTTPS://example.invalid/a";
    Check(editor->TryPrepareClipboardPaste(content, replacement) && replacement == L"[hello](<HTTPS://example.invalid/a>)",
          "URL schemes match case insensitively while preserving destination spelling");
    content.text = L"https://bad url";
    replacement  = L"sentinel";
    Check(! editor->TryPrepareClipboardPaste(content, replacement) && replacement == L"sentinel", "invalid URL retains ordinary paste");
    content.text                   = L"https://example.invalid/a";
    appearance.pasteUrlOnSelection = false;
    editor->SetAppearance(appearance);
    Check(! editor->TryPrepareClipboardPaste(content, replacement), "URL paste is opt in");
    content.htmlFragment    = L"<b>hello</b>";
    content.hasHtmlFragment = true;
    editor->SetHtmlPasteConverter([](std::wstring_view html) -> std::optional<std::wstring>
    { return html == L"<b>hello</b>" ? std::optional<std::wstring>{L"**hello**"} : std::nullopt; });
    Check(editor->TryPrepareClipboardPaste(content, replacement) && replacement == L"**hello**",
          "application HTML converter receives counted fragment and prepares Markdown");
    const auto originalRevision = editor->Revision();
    editor->SetHtmlPasteConverter([&](std::wstring_view) -> std::optional<std::wstring>
    {
        editor->SetText(L"changed during conversion");
        return L"stale";
    });
    replacement = L"sentinel";
    bool stale  = false;
    Check(! editor->TryPrepareClipboardPaste(content, replacement, &stale) && stale && replacement == L"sentinel" && editor->Revision() != originalRevision,
          "reentrant converter cannot publish a stale replacement");
    editor->SetHtmlPasteConverter([&](std::wstring_view) -> std::optional<std::wstring>
    {
        host.SetFocusControl(nullptr);
        return L"stale focus";
    });
    host.SetFocusControl(editor);
    stale = false;
    Check(! editor->TryPrepareClipboardPaste(content, replacement, &stale) && stale && replacement == L"sentinel",
          "reentrant focus replacement declines converter output");
    editor->SetHtmlPasteConverter({});
}
template <typename Prepare> void SourceHorizontalAndGuides(ControlHost& host, Prepare&& prepare)
{
    std::wstring source(180u, L'x');
    source += L" space\tend\r\nsecond line\n";
    auto root    = std::make_unique<RichTextEditor>(source);
    auto* editor = root.get();
    editor->SetViewMode(ViewMode::Source);
    auto appearance                  = editor->GetAppearance();
    appearance.sourceMonospace       = true;
    appearance.sourceWrap            = false;
    appearance.sourceLineNumbers     = true;
    appearance.sourceWhitespaceMarks = true;
    editor->SetAppearance(appearance);
    host.SetRoot(std::move(root));
    prepare();
    const auto initial = editor->GetHorizontalScrollInfo();
    Check(initial && initial->contentWidthDip > initial->viewportWidthDip + 100.0f, "unwrapped source exposes horizontal overflow");
    const auto before = editor->TryGetTextInputCaretRect(host, 30u);
    Check(before.has_value(), "source guide fixture has a prepared caret");
    const auto sourceRevision = editor->Revision();
    const auto selection      = editor->GetSelection();
    unsigned events{};
    editor->SetOnHorizontalScrollChanged([&](float) { ++events; });
    Check(editor->SetHorizontalScrollOffsetDip(80.0f), "source horizontal scrolling accepts a bounded offset");
    const auto after = editor->TryGetTextInputCaretRect(host, 30u);
    Check(after && std::abs((before->left - after->left) - 80.0f) < 0.1f && events == 1u,
          "horizontal scroll translates TSF/UIA caret geometry and sends one event");
    const auto hit = editor->TryHitTestTextInputPoint(host, D2D1::Point2F(after->left + 0.1f, (after->top + after->bottom) * 0.5f));
    Check(hit && *hit == 30u && editor->Revision() == sourceRevision && editor->GetSelection() == selection,
          "horizontal source hit testing preserves source, selection and revision");
    appearance.sourceWrap = true;
    editor->SetAppearance(appearance);
    prepare();
    const auto wrapped = editor->GetHorizontalScrollInfo();
    Check(wrapped && wrapped->offsetDip == 0.0f && wrapped->contentWidthDip <= wrapped->viewportWidthDip + 0.1f,
          "reenabling wrapping retires overflow and clamps the horizontal offset");
    const auto unzoomedViewport = editor->TryGetTextInputViewportRect();
    appearance.zoom             = 2.0f;
    editor->SetAppearance(appearance);
    prepare();
    const auto zoomedViewport = editor->TryGetTextInputViewportRect();
    Check(unzoomedViewport && zoomedViewport && zoomedViewport->left > unzoomedViewport->left && editor->GetText() == source,
          "source line-number lane grows for zoom without changing source");
    appearance.zoom       = 1.0f;
    appearance.sourceWrap = false;
    editor->SetAppearance(appearance);
    editor->SetFlowDirection(FlowDirection::RightToLeft);
    prepare();
    Check(editor->SetHorizontalScrollOffsetDip(0.0f), "reset RTL horizontal position");
    const auto rtlBefore = editor->TryGetTextInputCaretRect(host, 30u);
    Check(rtlBefore && editor->SetHorizontalScrollOffsetDip(80.0f), "RTL source admits horizontal scrolling");
    const auto rtlAfter = editor->TryGetTextInputCaretRect(host, 30u);
    Check(rtlAfter && std::abs((rtlAfter->left - rtlBefore->left) - 80.0f) < 0.1f, "RTL source moves caret geometry toward the opposite overflow edge");
    const auto viewport = editor->TryGetTextInputViewportRect();
    Check(viewport.has_value(), "RTL source has a viewport");
    bool resolved = false;
    for (std::size_t at = 0u; at < 180u && ! resolved; ++at)
        if (const auto caret = editor->TryGetTextInputCaretRect(host, at); caret && caret->left >= viewport->left && caret->left + 1.0f < viewport->right)
        {
            const auto point  = D2D1::Point2F(caret->left + 0.1f, (caret->top + caret->bottom) * 0.5f);
            const auto rtlHit = editor->TryHitTestTextInputPoint(host, point);
            resolved          = rtlHit && *rtlHit == at;
        }
    Check(resolved, "a visible RTL caret and hit test resolve the same source ACP after horizontal scrolling");
}
template <typename Prepare> void RichTableHorizontalOverflow(ControlHost& host, Prepare&& prepare)
{
    std::wstring source = L"| first | second |\n| --- | --- |\n| ";
    source += std::wstring(120u, L'w');
    source += L" | end |\n";
    auto root    = std::make_unique<RichTextEditor>(source);
    auto* editor = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    auto appearance                 = editor->GetAppearance();
    appearance.markdownPresentation = MarkdownPresentationStyle::Subtle;
    appearance.tableOverflow        = true;
    editor->SetAppearance(appearance);
    host.SetRoot(std::move(root));
    prepare();
    const auto info = editor->GetHorizontalScrollInfo();
    Check(info && info->contentWidthDip > info->viewportWidthDip + 100.0f, "intrinsic table exposes its retained horizontal extent");
    const auto before = editor->TryGetTextInputCaretRect(host, source.find(L"ww"));
    Check(before && editor->SetHorizontalScrollOffsetDip(80.0f), "overflow table accepts horizontal offset");
    const auto after = editor->TryGetTextInputCaretRect(host, source.find(L"ww"));
    Check(after && std::abs((before->left - after->left) - 80.0f) < 0.1f && editor->GetText() == source,
          "overflow scrolling translates table source geometry without editing source");
}
template <typename Prepare> void SemanticGeometryReuse(ControlHost& host, Prepare&& prepare)
{
    std::wstring source;
    for (unsigned i = 0u; i < 100u; ++i)
        source += L"Paragraph **bold** text.\n\n";
    auto root    = std::make_unique<RichTextEditor>(source);
    auto* editor = root.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    host.SetRoot(std::move(root));
    prepare();
    Check(editor->GetAcknowledgedSemanticDocument(host) && editor->DebugGetState().reusedGeometryChunks == 0u,
          "first semantic publication creates its layouts");
    editor->SetSelection({4u, 4u});
    Check(editor->ReplaceSelection(L"x"), "edit one early paragraph");
    prepare();
    Check(editor->GetAcknowledgedSemanticDocument(host) && editor->DebugGetState().reusedGeometryChunks >= 90u &&
              editor->GetText().size() == source.size() + 1u,
          "unchanged source blocks reuse immutable DirectWrite layouts after a tracked edit");
    const auto firstRevision = editor->Revision();
    editor->SetSelection({7u, 7u});
    Check(editor->ReplaceSelection(L"y") && ! editor->GetAcknowledgedSemanticDocument(host),
          "a second source edit retires stale semantic geometry before preparation");
    editor->SetSelection({8u, 8u});
    Check(editor->ReplaceSelection(L"z") && editor->Revision() == firstRevision + 2u, "superseding edits remain separate source transactions");
    prepare();
    Check(editor->GetAcknowledgedSemanticDocument(host) && editor->DebugGetState().reusedGeometryChunks >= 90u,
          "latest publication reuses only compatible boxes after cancelled source preparation");
    auto appearance            = editor->GetAppearance();
    appearance.bodyFontSizeDip = 19.f;
    editor->SetAppearance(appearance);
    prepare();
    Check(editor->DebugGetState().reusedGeometryChunks == 0u, "typography changes never reuse incompatible semantic layouts");
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
