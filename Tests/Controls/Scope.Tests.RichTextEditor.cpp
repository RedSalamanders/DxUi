#include "Scope.Tests.RichTextEditor.h"
#include "../../src/Controls/TextClipboard.h"
#include "../Support/Scope.Tests.ClipboardFixture.h"
#include "../Support/Scope.Tests.Footnotes.h"
#include "../Support/Scope.Tests.MarkdownBlocks.h"
#include "../Support/Scope.Tests.MarkdownCapacity.h"
#include "../Support/Scope.Tests.MarkdownInlines.h"
#include "../Support/Scope.Tests.RichImage.h"
#include "../Support/Scope.Tests.RichImagePaths.h"
#include "../Support/Scope.Tests.RichImageResources.h"
#include "../Support/Scope.Tests.SemanticDiagrams.h"
#include "../Support/Scope.Tests.SemanticSourceBlock.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Scope.Tests.RichMarkdown.h"
#include <DxUi/ControlCatalog.h>
#include <DxUi/RichSemanticDocument.h>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace DxUi;
using namespace DxUi::RichText;

class RichEditorClipboard final : public TextClipboard
{
public:
    RichEditorClipboard() : previous(testTextClipboard)
    {
        testTextClipboard = this;
    }
    ~RichEditorClipboard()
    {
        testTextClipboard = previous;
    }
    TextClipboard* previous;
    std::wstring text;
    bool busy = false;
    std::function<void()> afterRead;
    HRESULT Read(HWND, std::wstring& out) noexcept override
    {
        if (busy)
            return E_ACCESSDENIED;
        try
        {
            out = text;
            if (auto callback = std::exchange(afterRead, {}))
                callback();
            return S_OK;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
    }
    HRESULT Write(HWND, std::wstring_view value) noexcept override
    {
        if (busy)
            return E_ACCESSDENIED;
        try
        {
            text = value;
            return S_OK;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
    }
};

template <typename F> void RejectEditor(F&& operation)
{
    bool rejected = false;
    try
    {
        operation();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    Require(rejected, "invalid editor update is rejected");
}

void RenderEditor(AttachedHostWindow& window)
{
    Require(PrepareNativeContentForTests(window.Host()), "native retained rich-text owner turns finish preparation");
}

void TestRichTextEditorTransactionsAndValidation()
{
    ControlHost host;
    RichTextEditor editor(L"abc");
    size_t changes = 0;
    editor.SetOnTextChanged([&] { ++changes; });
    editor.SetSelection({3, 1});
    Require(editor.ReplaceSelection(L"\U0001f600"), "replace backwards selection");
    Require(editor.GetText() == L"a\U0001f600" && editor.GetSelection() == Selection{3, 3}, "source transaction and caret");
    Require(editor.Undo() && editor.GetText() == L"abc" && editor.GetSelection() == Selection{3, 1}, "undo restores source and direction");
    Require(editor.Redo() && editor.GetText() == L"a\U0001f600" && changes == 3, "redo and one notification per change");
    const auto revision = editor.Revision();
    const auto snapshot = editor.Snapshot();
    RejectEditor([&] { editor.SetSelection({2, 2}); });
    RejectEditor([&] { editor.SetText(std::wstring(1, wchar_t(0xd800))); });
    RejectEditor([&] { editor.ReplaceSelection(std::wstring(Document::MaximumUnits + 1, L'x')); });
    RejectEditor([&] { editor.SetViewMode(static_cast<ViewMode>(99)); });
    Require(editor.Revision() == revision && snapshot.Text() == editor.GetText() && changes == 3, "rejections preserve source history and callbacks");
    editor.SetReadOnly(true);
    Require(! editor.ReplaceSelection(L"x") && ! editor.Undo() && ! editor.Redo() && ! editor.ToggleFormatting(L"**"), "read-only blocks source commands");
    Require(! editor.OnChar(host, L'x', 0), "read-only rejects character input");
    Require(editor.OnSelectAll(host) && editor.GetSelection() == Selection{0, 3}, "read-only retains selection");
    editor.SetReadOnly(false);
    editor.SetEnabled(false);
    Require(! editor.ReplaceSelection(L"x") && ! editor.OnSelectAll(host) && ! editor.OnKeyDown(host, VK_DELETE, 0), "disabled blocks interaction");
    editor.SetEnabled(true);
    editor.SetText(L"new");
    Require(! editor.CanUndo() && ! editor.CanRedo() && changes == 3, "SetText resets history without application notification");
}

void TestRichTextEditorUnicodeCharacterTransactions()
{
    ControlHost host;
    RichTextEditor editor;
    size_t changes = 0;
    editor.SetOnTextChanged([&] { ++changes; });
    Require(editor.OnChar(host, wchar_t(0xd83d), 0) && editor.GetText().empty(), "pending lead surrogate stays outside document");
    Require(editor.OnChar(host, wchar_t(0xde00), 0) && editor.GetText() == L"\U0001f600" && changes == 1, "one scalar is one edit");
    Require(editor.Undo() && editor.GetText().empty(), "one undo removes complete scalar");
    Require(editor.OnChar(host, wchar_t(0xde00), 0) && editor.GetText().empty(), "orphan trail discarded");
    static_cast<void>(editor.OnChar(host, wchar_t(0xd83d), 0));
    editor.SetSelection({0, 0});
    static_cast<void>(editor.OnChar(host, wchar_t(0xde00), 0));
    Require(editor.GetText().empty(), "selection change retires pending scalar");
    static_cast<void>(editor.OnChar(host, wchar_t(0xd83d), 0));
    Require(editor.OnChar(host, L'\r', 0) && editor.GetText() == L"\r\n", "Enter commits one CRLF transaction");
    Require(! editor.OnChar(host, L'\t', 0) && ! editor.OnChar(host, L'\0', 0), "host traversal and NUL never enter source");
    Require(! editor.OnChar(host, L'b', MK_CONTROL) && ! editor.OnChar(host, L'i', kModifierAlt), "shortcuts do not insert characters");
    Require(editor.OnChar(host, L'\u20ac', MK_CONTROL | kModifierAlt) && editor.GetText() == L"\r\n\u20ac", "AltGr Unicode character is accepted");
    Require(editor.Undo() && editor.GetText() == L"\r\n", "AltGr insertion is an ordinary source transaction");
    static_cast<void>(editor.OnChar(host, wchar_t(0xd83d), 0));
    editor.SetReadOnly(true);
    editor.SetReadOnly(false);
    static_cast<void>(editor.OnChar(host, wchar_t(0xde00), 0));
    Require(editor.GetText() == L"\r\n" && IsValidUtf16(editor.GetText()), "mode transition retires pending scalar");
}

void TestRichTextEditorFormattingAndModePreserveSource()
{
    ControlHost host;
    RichTextEditor editor(L"hello");
    editor.SetSelection({5, 0});
    Require(editor.OnKeyDown(host, 'B', MK_CONTROL), "bold command consumed");
    Require(editor.GetText() == L"**hello**" && editor.GetSelection() == Selection{7, 2}, "bold preserves backwards source selection");
    const auto revision = editor.Revision();
    editor.SetViewMode(ViewMode::Source);
    editor.SetViewMode(ViewMode::Rich);
    Require(editor.Revision() == revision && editor.GetText() == L"**hello**", "view mode never rewrites source or history");
    Require(editor.ToggleFormatting(L"**") && editor.GetText() == L"hello", "toggle unwraps exact delimiter");
    Require(editor.ToggleFormatting(L"*") && editor.ToggleFormatting(L"~~") && editor.ToggleFormatting(L"`"), "all four v0 formatting commands");
    Require(editor.Undo() && editor.Redo(), "formatting is undoable");
    const auto source = editor.GetText();
    RejectEditor([&] { editor.ToggleFormatting(L"unsupported"); });
    Require(editor.GetText() == source, "unsupported formatting preserves source");
}

void TestRichTextEditorCallbackRetirementAndFailure()
{
    ControlHost host;
    auto editor = std::make_unique<RichTextEditor>(L"x");
    auto* raw   = editor.get();
    host.SetRoot(std::move(editor));
    size_t calls = 0;
    raw->SetOnTextChanged([&]
    {
        ++calls;
        host.SetRoot(std::make_unique<Label>(L"replacement"));
    });
    Require(raw->OnChar(host, L'y', 0) && calls == 1, "callback may retire editor during character command");
    auto replacement = std::make_unique<RichTextEditor>();
    raw              = replacement.get();
    host.SetRoot(std::move(replacement));
    raw->SetOnTextChanged([] { throw std::runtime_error("notification failed"); });
    Require(raw->OnChar(host, L'z', 0) && raw->GetText() == L"z" && raw->CanUndo(), "callback failure leaves committed edit usable");
    raw->SetOnTextChanged([&]
    {
        raw->SetOnTextChanged({});
        raw->SetText(L"application replacement");
    });
    Require(raw->ReplaceSelection(L"!") && raw->GetText() == L"application replacement" && ! raw->CanUndo(), "callback may replace source and delegate");
}

void TestRichTextEditorSelectionPublicationAndRetirement()
{
    ControlHost host;
    RichTextEditor editor(L"alpha");
    std::vector<Selection> selections;
    editor.SetOnSelectionChanged([&] { selections.push_back(editor.GetSelection()); });
    Require(selections.empty() && ! editor.IsSelectionGestureActive(), "selection registration is silent without a pointer gesture");
    editor.SetSelection({2u, 2u});
    editor.SetSelection({2u, 2u});
    Require(selections == std::vector<Selection>{{2u, 2u}}, "programmatic publication suppresses unchanged endpoints");
    Require(editor.OnKeyDown(host, VK_RIGHT, 0u) && selections.back() == Selection{3u, 3u} && selections.size() == 2u,
            "keyboard source selection notifies synchronously");
    editor.SetOnSelectionChanged([&]
    {
        selections.push_back(editor.GetSelection());
        if (editor.GetSelection().caret == 1u)
            editor.SetSelection({4u, 4u});
    });
    editor.SetSelection({1u, 1u});
    Require(selections.size() == 4u && selections[2] == Selection{1u, 1u} && selections[3] == Selection{4u, 4u},
            "reentrant publication observes each committed predecessor exactly once");
    editor.SetOnSelectionChanged([&] { editor.SetOnSelectionChanged({}); });
    editor.SetSelection({0u, 0u});
    editor.SetSelection({1u, 1u});
    editor.SetOnSelectionChanged([] { throw std::runtime_error("selection notification failed"); });
    editor.SetSelection({2u, 2u});
    Require(editor.GetSelection() == Selection{2u, 2u} && ! editor.CanUndo(), "notification failure preserves committed selection and history");

    auto owned = std::make_unique<RichTextEditor>(L"owned");
    auto* raw  = owned.get();
    host.SetRoot(std::move(owned));
    size_t calls = 0u;
    raw->SetOnSelectionChanged([&]
    {
        ++calls;
        host.SetRoot(std::make_unique<Label>(L"replacement"));
    });
    raw->SetSelection({1u, 1u});
    Require(calls == 1u, "selection callback may retire its editor synchronously");
}

void TestRichTextEditorCommittedTextThenSelectionCallbacks()
{
    RichTextEditor editor(L"abc");
    std::vector<char> events;
    std::vector<Selection> selections;
    editor.SetOnTextChanged([&] { events.push_back('T'); });
    editor.SetOnSelectionChanged([&]
    {
        events.push_back('S');
        selections.push_back(editor.GetSelection());
    });
    editor.SetSelection({1u, 1u});
    events.clear();
    selections.clear();
    Require(editor.ReplaceSelection(L"x") && events == std::vector<char>{'T', 'S'} && selections == std::vector<Selection>{{2u, 2u}},
            "committed edit notifies text consumer before final selection consumer");
    events.clear();
    selections.clear();
    Require(editor.Undo() && events == std::vector<char>{'T', 'S'} && selections == std::vector<Selection>{{1u, 1u}},
            "undo publishes committed source before its restored selection");

    events.clear();
    selections.clear();
    editor.SetOnTextChanged([&]
    {
        events.push_back('T');
        editor.SetOnTextChanged({});
    });
    Require(editor.Redo() && events == std::vector<char>{'T', 'S'} && selections == std::vector<Selection>{{2u, 2u}},
            "self-clearing text callback does not suppress its surviving selection publication");

    RichTextEditor throwing(L"ab");
    throwing.SetSelection({1u, 1u});
    std::vector<char> failedCallbackEvents;
    throwing.SetOnTextChanged([&]
    {
        failedCallbackEvents.push_back('T');
        throw std::runtime_error("planned committed text callback failure");
    });
    throwing.SetOnSelectionChanged([&] { failedCallbackEvents.push_back('S'); });
    Require(throwing.ReplaceSelection(L"x") && throwing.GetText() == L"axb" && throwing.CanUndo() && failedCallbackEvents == std::vector<char>{'T', 'S'},
            "standard text callback failure retains committed source/history and still publishes selection");

    RichTextEditor replacedRegistration(L"abc");
    replacedRegistration.SetSelection({1u, 1u});
    unsigned oldSelectionCalls = 0u, newSelectionCalls = 0u;
    replacedRegistration.SetOnSelectionChanged([&] { ++oldSelectionCalls; });
    replacedRegistration.SetOnTextChanged([&] { replacedRegistration.SetOnSelectionChanged([&] { ++newSelectionCalls; }); });
    Require(replacedRegistration.ReplaceSelection(L"x") && oldSelectionCalls == 0u && newSelectionCalls == 0u,
            "selection callback replacement remains silent and does not receive an already-current registration event");
    replacedRegistration.SetSelection({0u, 0u});
    Require(newSelectionCalls == 1u, "replacement selection callback receives the next actual selection change");

    RichTextEditor nested(L"abcd");
    nested.SetSelection({1u, 1u});
    std::vector<char> nestedEvents;
    std::vector<Selection> nestedSelections;
    unsigned callbackDepth = 0u;
    nested.SetOnTextChanged([&]
    {
        nestedEvents.push_back('T');
        if (callbackDepth++ == 0u)
            Require(nested.ReplaceSelection(L"y"), "text callback can commit a nested edit");
        --callbackDepth;
    });
    nested.SetOnSelectionChanged([&]
    {
        nestedEvents.push_back('S');
        nestedSelections.push_back(nested.GetSelection());
    });
    Require(nested.ReplaceSelection(L"x") && nested.GetText() == L"axybcd" && nestedEvents == std::vector<char>{'T', 'T', 'S'} &&
                nestedSelections == std::vector<Selection>{{3u, 3u}},
            "nested source revision publishes only its final selection and suppresses stale outer publication");

    RichTextEditor movedSelection(L"abc");
    movedSelection.SetSelection({1u, 1u});
    std::vector<char> movedEvents;
    movedSelection.SetOnSelectionChanged([&] { movedEvents.push_back('S'); });
    movedSelection.SetOnTextChanged([&]
    {
        movedEvents.push_back('T');
        movedSelection.SetSelection({0u, 0u});
    });
    Require(movedSelection.ReplaceSelection(L"x") && movedEvents == std::vector<char>{'T', 'S'} && movedSelection.GetSelection() == Selection{0u, 0u},
            "selection mutation inside text callback publishes once and prevents stale duplicate selection");

    class TextServiceEditor final : public RichTextEditor
    {
    public:
        using RichTextEditor::ExportTextInputState;
        using RichTextEditor::ImportTextInputState;
        using RichTextEditor::RichTextEditor;
    };
    ControlHost textHost;
    auto textOwner   = std::make_unique<TextServiceEditor>(L"abc");
    auto* textEditor = textOwner.get();
    textHost.SetRoot(std::move(textOwner));
    textEditor->SetSelection({1u, 1u});
    TextInputState committed;
    Require(textEditor->ExportTextInputState(committed), "text-service baseline exports");
    auto preview                 = committed;
    preview.text                 = L"aXbc";
    preview.caretIndex           = 2u;
    preview.selectionAnchorIndex = 2u;
    std::vector<char> serviceEvents;
    textEditor->SetOnTextChanged([&] { serviceEvents.push_back('T'); });
    textEditor->SetOnSelectionChanged([&] { serviceEvents.push_back('S'); });
    Require(textEditor->ImportTextInputState(textHost, preview, false) && textEditor->GetText() == L"aXbc" && serviceEvents == std::vector<char>{'S'} &&
                ! textEditor->CanUndo(),
            "TSF preview remains immediate and does not become a committed text/history event");
    serviceEvents.clear();
    Require(textEditor->ImportTextInputState(textHost, committed, false) && textEditor->GetText() == L"abc" && serviceEvents == std::vector<char>{'S'} &&
                ! textEditor->CanUndo(),
            "TSF cancellation immediately restores selection/source without history notification");
    serviceEvents.clear();
    Require(textEditor->ImportTextInputState(textHost, preview, true) && textEditor->GetText() == L"aXbc" && textEditor->CanUndo() &&
                serviceEvents == std::vector<char>{'T', 'S'},
            "TSF commit uses committed text-then-selection callback ordering exactly once");

    ControlHost repeatedPreviewHost;
    auto repeatedPreviewOwner = std::make_unique<TextServiceEditor>(L"abcX");
    auto* repeatedPreview     = repeatedPreviewOwner.get();
    repeatedPreviewHost.SetRoot(std::move(repeatedPreviewOwner));
    repeatedPreview->SetSelection({4u, 4u});
    TextInputState repeatedCommitted;
    Require(repeatedPreview->ExportTextInputState(repeatedCommitted), "same-anchor TSF baseline exports");
    std::vector<char> repeatedEvents;
    std::vector<Selection> repeatedSelections;
    repeatedPreview->SetOnTextChanged([&] { repeatedEvents.push_back('T'); });
    repeatedPreview->SetOnSelectionChanged([&]
    {
        repeatedEvents.push_back('S');
        repeatedSelections.push_back(repeatedPreview->GetSelection());
    });
    Require(repeatedEvents.empty(), "same-anchor callback registration remains silent");
    auto repeatedState                 = repeatedCommitted;
    repeatedState.text                 = L"abcY";
    repeatedState.caretIndex           = 4u;
    repeatedState.selectionAnchorIndex = 4u;
    Require(repeatedPreview->ImportTextInputState(repeatedPreviewHost, repeatedState, false) && repeatedPreview->GetText() == L"abcY" &&
                repeatedEvents == std::vector<char>{'S'} && repeatedSelections == std::vector<Selection>{{4u, 4u}} && ! repeatedPreview->CanUndo() &&
                ! repeatedPreview->CanRedo(),
            "first non-notifying source revision publishes selection at the unchanged ACP without committed history");
    Require(repeatedPreview->ImportTextInputState(repeatedPreviewHost, repeatedState, false) && repeatedEvents == std::vector<char>{'S'},
            "same source revision and ACP suppress a duplicate text-service notification");
    repeatedState.text = L"abcZ";
    Require(repeatedPreview->ImportTextInputState(repeatedPreviewHost, repeatedState, false) && repeatedPreview->GetText() == L"abcZ" &&
                repeatedEvents == std::vector<char>{'S', 'S'} && repeatedSelections == std::vector<Selection>{{4u, 4u}, {4u, 4u}} &&
                ! repeatedPreview->CanUndo() && ! repeatedPreview->CanRedo(),
            "second non-notifying source revision publishes even when ACP and gesture state are unchanged");
    Require(repeatedPreview->ImportTextInputState(repeatedPreviewHost, repeatedCommitted, false) && repeatedPreview->GetText() == L"abcX" &&
                repeatedEvents == std::vector<char>{'S', 'S', 'S'} && repeatedSelections.back() == Selection{4u, 4u} && ! repeatedPreview->CanUndo() &&
                ! repeatedPreview->CanRedo(),
            "cancellation publishes its restored source revision without creating history");
    Require(repeatedPreview->ImportTextInputState(repeatedPreviewHost, repeatedCommitted, false) && repeatedEvents.size() == 3u,
            "repeated unchanged cancellation remains silent");
    auto repeatedCommit = repeatedCommitted;
    repeatedCommit.text = L"abcW";
    Require(repeatedPreview->ImportTextInputState(repeatedPreviewHost, repeatedCommit, true) && repeatedPreview->GetText() == L"abcW" &&
                repeatedPreview->CanUndo() && repeatedEvents == std::vector<char>{'S', 'S', 'S', 'T', 'S'} && repeatedSelections.back() == Selection{4u, 4u},
            "notifying commit retains text-then-selection order after same-anchor transient revisions");

    serviceEvents.clear();
    textEditor->SetSelection({4u, 4u});
    serviceEvents.clear();
    textEditor->SetText(L"x");
    Require(serviceEvents == std::vector<char>{'S'}, "silent SetText keeps its selection-only notification and never reports committed text");

    ControlHost firstHost, secondHost;
    auto panelOwner = std::make_unique<Panel>();
    auto* panel     = panelOwner.get();
    auto* rebound   = panelOwner->AddChild<RichTextEditor>();
    rebound->SetText(L"kept");
    rebound->SetSelection({4u, 4u});
    firstHost.SetRoot(std::move(panelOwner));
    unsigned reboundSelectionCalls = 0u;
    rebound->SetOnTextChanged([&]
    {
        auto child = panel->TakeChild(0u);
        secondHost.SetRoot(std::move(child));
    });
    rebound->SetOnSelectionChanged([&] { ++reboundSelectionCalls; });
    Require(rebound->ReplaceSelection(L"!") && secondHost.GetRoot() == rebound && rebound->GetText() == L"kept!" && rebound->CanUndo() &&
                reboundSelectionCalls == 0u,
            "same editor rebind during text callback suppresses the stale outer selection while preserving source/history");
    rebound->SetOnTextChanged({});
    rebound->SetSelection({0u, 0u});
    Require(reboundSelectionCalls == 1u, "rebound editor publishes later selection changes through its current host");

    ControlHost retirementHost;
    auto retiring        = std::make_unique<RichTextEditor>(L"abc");
    auto* retiringEditor = retiring.get();
    retirementHost.SetRoot(std::move(retiring));
    unsigned staleSelections = 0u;
    retiringEditor->SetSelection({1u, 1u});
    retiringEditor->SetOnTextChanged([&] { retirementHost.SetRoot(std::make_unique<Label>(L"replacement")); });
    retiringEditor->SetOnSelectionChanged([&] { ++staleSelections; });
    Require(retiringEditor->ReplaceSelection(L"x") && dynamic_cast<Label*>(retirementHost.GetRoot()) && staleSelections == 0u,
            "text callback retirement prevents a deferred selection access to the former editor");
}

void TestRichTextEditorSelectionGestureAndTextServicePublication()
{
    class SelectionAdapterEditor final : public RichTextEditor
    {
    public:
        using RichTextEditor::ExportTextInputState;
        using RichTextEditor::ImportTextInputState;
        using RichTextEditor::OnCaptureLost;
        using RichTextEditor::RichTextEditor;
    };
    AttachedHostWindow window;
    auto owned   = std::make_unique<SelectionAdapterEditor>(L"alpha beta gamma");
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    RenderEditor(window);
    window.Host().SetFocusControl(editor, false);
    std::vector<std::pair<Selection, bool>> notifications;
    editor->SetOnSelectionChanged([&] { notifications.emplace_back(editor->GetSelection(), editor->IsSelectionGestureActive()); });
    const auto point = [&](size_t source)
    {
        const auto rectangle = editor->TryGetTextInputCaretRect(window.Host(), source);
        Require(rectangle.has_value(), "prepared selection gesture caret geometry");
        return D2D1::Point2F(rectangle->left + .1f, (rectangle->top + rectangle->bottom) * .5f);
    };
    const auto start = point(0u), end = point(8u);
    Require(editor->OnMouseDown(window.Host(), start, false, 0u) && notifications.size() == 1u && notifications.back().second,
            "pointer begin notifies active gesture even at unchanged insertion point");
    Require(editor->OnMouseMove(window.Host(), end, 0u) && notifications.size() == 2u && notifications.back().second,
            "pointer movement publishes source endpoints with active gesture state");
    const auto selected = editor->GetSelection();
    static_cast<void>(editor->OnMouseMove(window.Host(), end, 0u));
    Require(notifications.size() == 2u && selected.anchor != selected.caret, "steady pointer selection suppresses duplicate notification");
    Require(editor->OnMouseUp(window.Host(), end, false, 0u) && notifications.size() == 3u && ! notifications.back().second &&
                notifications.back().first == selected,
            "pointer release notifies inactive gesture without changing endpoints");
    static_cast<void>(editor->OnMouseUp(window.Host(), end, false, 0u));
    Require(notifications.size() == 3u, "repeated release cannot duplicate the completion callback");
    Require(editor->OnMouseDown(window.Host(), end, false, 0u), "begin capture cancellation fixture");
    editor->OnCaptureLost(window.Host());
    Require(notifications.size() == 5u && ! notifications.back().second, "capture cancellation publishes final inactive gesture");
    window.Host().ReleaseMouseCapture();
    Require(notifications.size() == 5u, "host capture cleanup suppresses duplicate cancellation");
    Require(editor->OnMouseDown(window.Host(), end, false, 0u) && editor->OnKeyDown(window.Host(), VK_ESCAPE, 0u) && notifications.size() == 7u &&
                ! notifications.back().second,
            "Escape completes pointer selection without an endpoint change");
    TextInputState state;
    Require(editor->ExportTextInputState(state), "export canonical text service state");
    state.caretIndex           = 2u;
    state.selectionAnchorIndex = 1u;
    Require(editor->ImportTextInputState(window.Host(), state, false) && notifications.size() == 8u &&
                notifications.back() == std::pair{Selection{1u, 2u}, false},
            "text service selection-only import publishes through the same callback");
    Require(editor->ImportTextInputState(window.Host(), state, false) && notifications.size() == 8u && ! editor->CanUndo(),
            "unchanged native import preserves duplicate suppression and source history");
    editor->SetOnSelectionChanged([&]
    {
        if (! editor->IsSelectionGestureActive())
            window.Host().SetRoot(std::make_unique<Label>(L"replacement"));
    });
    Require(editor->OnMouseDown(window.Host(), end, false, 0u), "begin pointer release retirement fixture");
    Require(editor->OnMouseUp(window.Host(), end, false, 0u), "final pointer callback may destroy its editor after publication");
}

void TestRichTextEditorNativeHostAndLayoutReuse()
{
    AttachedHostWindow window;
    auto editor = std::make_unique<RichTextEditor>(L"# Heading\r\n**bold** \u03b2 \U0001f600\nlast");
    auto* raw   = editor.get();
    window.Host().SetRoot(std::move(editor));
    RenderEditor(window);
    const auto prepared = raw->DebugGetState();
    Require(prepared.layoutReady && prepared.contentHeightDip > 0, "native host supplies DWrite and raster services");
    RenderEditor(window);
    Require(raw->DebugGetState().layoutGeneration == prepared.layoutGeneration, "unchanged painting reuses layout");
    raw->SetSelection({2, 9});
    RenderEditor(window);
    Require(raw->DebugGetState().layoutGeneration == prepared.layoutGeneration, "selection only rerasterizes pixels");
    auto palette   = window.Host().GetTheme();
    palette.accent = D2D1::ColorF(D2D1::ColorF::Orange);
    window.Host().SetTheme(palette);
    RenderEditor(window);
    Require(raw->DebugGetState().layoutGeneration == prepared.layoutGeneration, "palette update reuses shaping");
    raw->SetFlowDirection(FlowDirection::RightToLeft);
    RenderEditor(window);
    Require(raw->DebugGetState().layoutGeneration > prepared.layoutGeneration && raw->GetText().starts_with(L"# Heading"),
            "flow rebuilds layout without changing source");
    raw->SetVisible(false);
    RenderEditor(window);
    Require(! raw->DebugGetState().layoutReady, "hidden branch releases retained layout");
    raw->SetVisible(true);
    RenderEditor(window);
    Require(raw->DebugGetState().layoutReady, "show prepares layout again");
}

void TestRichTextEditorDocumentFocusChromeDoesNotRetireTextGeometry()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>(L"A quiet writing surface.");
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    auto appearance       = editor->GetAppearance();
    appearance.drawBorder = false;
    editor->SetAppearance(appearance);
    window.Host().SetFocusControl(editor, true);
    RenderEditor(window);
    const auto source     = editor->Snapshot();
    const auto selection  = editor->GetSelection();
    const auto generation = editor->DebugGetState().layoutGeneration;
    WindowHostBitmapCapture framed, quiet;
    Require(window.Host().DebugCaptureBitmap(framed), "capture keyboard-focused borderless editor");
    appearance.drawFocusIndicator = false;
    editor->SetAppearance(appearance);
    RenderEditor(window);
    Require(window.Host().DebugCaptureBitmap(quiet) && quiet.bgraPixels != framed.bgraPixels,
            "a document surface can omit the decorative perimeter while retaining its caret");
    Require(editor->Snapshot().IsSameRevision(source) && editor->GetSelection() == selection && ! editor->CanUndo() &&
                editor->DebugGetState().layoutGeneration == generation,
            "focus presentation changes no source, selection, history or prepared geometry");
    auto theme         = window.Host().GetTheme();
    theme.highContrast = true;
    window.Host().SetTheme(theme);
    RenderEditor(window);
    Require(window.Host().DebugCaptureBitmap(quiet), "capture document surface in high contrast");
    appearance.drawFocusIndicator = true;
    editor->SetAppearance(appearance);
    RenderEditor(window);
    Require(window.Host().DebugCaptureBitmap(framed) && framed.bgraPixels == quiet.bgraPixels,
            "high contrast always retains the same focus indicator regardless of the document opt-out");
}

void TestRichTextEditorPreparedSourceInkPreservesGeometryAndSelection()
{
    AttachedHostWindow window;
    const std::wstring source = L"# Heading\r\n\r\n- **strong** [label](https://example.invalid/path)\r\n";
    auto owned                = std::make_unique<RichTextEditor>(source);
    auto* editor              = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::CommonMark0312);
    editor->SetViewMode(ViewMode::Source);
    window.Host().SetRoot(std::move(owned));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    RenderEditor(window);
    WindowHostBitmapCapture plain, colored, selectedColored, selectedPlain;
    Require(window.Host().DebugCaptureBitmap(plain), "ordinary source fixture capture without activation");
    const auto geometry               = editor->DebugGetState().layoutGeneration;
    const auto before                 = editor->TryGetTextInputCaretRect(window.Host(), source.find(L"strong"));
    auto appearance                   = editor->GetAppearance();
    appearance.colorizeMarkdownSource = true;
    editor->SetAppearance(appearance);
    RenderEditor(window);
    const auto after = editor->TryGetTextInputCaretRect(window.Host(), source.find(L"strong"));
    Require(editor->DebugGetState().sourceInkRangeCount > 0u && ! editor->DebugGetState().sourceInkPreparationPending &&
                editor->DebugGetState().layoutGeneration == geometry && before && after && before->left == after->left && before->top == after->top &&
                before->bottom == after->bottom && editor->GetText() == source && ! editor->CanUndo(),
            "source ink publication preserves exact source/SDK geometry and history");
    Require(window.Host().DebugCaptureBitmap(colored) && colored.bgraPixels != plain.bgraPixels, "prepared source roles change actual glyph ink");
    editor->SetFocusModeEnabled(true);
    RenderEditor(window);
    WindowHostBitmapCapture sourceFocused;
    Require(window.Host().DebugCaptureBitmap(sourceFocused) && sourceFocused.bgraPixels == colored.bgraPixels, "Source view never receives focus dimming");
    editor->SetSelection({0u, source.size()});
    RenderEditor(window);
    Require(window.Host().DebugCaptureBitmap(selectedColored), "selected colored source capture");
    appearance.colorizeMarkdownSource = false;
    editor->SetAppearance(appearance);
    RenderEditor(window);
    Require(window.Host().DebugCaptureBitmap(selectedPlain) && selectedPlain.bgraPixels == selectedColored.bgraPixels,
            "true selection colors override all custom source roles");
    editor->SetSelection({0u, 0u});
    auto palette         = window.Host().GetTheme();
    palette.highContrast = true;
    window.Host().SetTheme(palette);
    RenderEditor(window);
    Require(window.Host().DebugCaptureBitmap(plain), "high contrast source capture");
    appearance.colorizeMarkdownSource = true;
    editor->SetAppearance(appearance);
    RenderEditor(window);
    Require(window.Host().DebugCaptureBitmap(colored) && colored.bgraPixels == plain.bgraPixels,
            "high contrast suppresses custom syntax ink using the shared ordinary text pair");
    editor->SetText(L"replacement");
    Require(editor->DebugGetState().sourceInkRangeCount == 0u, "source replacement immediately retires all old ink");
}

void TestRichTextEditorNativeUnicodeRoutingAndNavigation()
{
    AttachedHostWindow window;
    auto editor = std::make_unique<RichTextEditor>(L"a\u0301\U0001f600\r\nword");
    auto* raw   = editor.get();
    raw->SetViewMode(ViewMode::Source);
    window.Host().SetRoot(std::move(editor));
    RenderEditor(window);
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.Host().SetFocusControl(raw, false); // Logical focus only: no desktop takeover.
    Require(window.Host().HasActiveNativeTextInputSession(), "D2d maintains a logical native text cache without taking desktop focus");
    raw->SetSelection({4, 4});
    Require(raw->OnKeyDown(window.Host(), VK_LEFT, 0) && raw->GetSelection().caret == 2, "left crosses complete supplementary scalar");
    Require(raw->OnKeyDown(window.Host(), VK_LEFT, 0) && raw->GetSelection().caret == 0, "left crosses combining cluster");
    Require(raw->OnKeyDown(window.Host(), VK_RIGHT, MK_SHIFT) && raw->GetSelection() == Selection{0, 2}, "shift navigation extends source selection");
    Require(raw->OnKeyDown(window.Host(), VK_BACK, 0) && raw->GetText() == L"\U0001f600\r\nword", "delete combining selection atomically");
    raw->SetSelection({2, 2});
    Require(raw->OnKeyDown(window.Host(), VK_DELETE, 0) && raw->GetText() == L"\U0001f600word", "display newline deletion preserves CRLF boundary");
    Require(raw->OnKeyDown(window.Host(), VK_END, MK_CONTROL), "document end command");
    SendMessageW(window.Hwnd(), WM_CHAR, L'\u03b2', 0);
    Require(raw->GetText() == L"\U0001f600word\u03b2", "native WM_CHAR routes through retained control");
    Require(raw->OnKeyDown(window.Host(), VK_HOME, MK_CONTROL) && raw->GetSelection().caret == 0, "document home");
    Require(! raw->OnKeyDown(window.Host(), VK_TAB, 0), "Tab remains host navigation");
    raw->SetText(L"one two\r\nthree\r\nfour");
    Require(raw->OnKeyDown(window.Host(), VK_RIGHT, MK_CONTROL) && raw->GetSelection().caret == 4, "word navigation uses existing host text helper");
    raw->SetSelection({0, 0});
    const auto pendingAdvances = raw->DebugGetState().preparationAdvances;
    Require(raw->OnKeyDown(window.Host(), VK_DOWN, 0) && raw->GetSelection().caret == 0 && raw->DebugGetState().preparationAdvances == pendingAdvances,
            "vertical movement waits for visual lines without draining preparation");
    RenderEditor(window);
    Require(raw->OnKeyDown(window.Host(), VK_DOWN, 0) && raw->GetSelection().caret > 0, "down enters next prepared line");
    Require(raw->OnKeyDown(window.Host(), VK_UP, 0) && raw->GetSelection().caret == 0, "up retains preferred horizontal position");
    Require(raw->OnKeyDown(window.Host(), VK_NEXT, 0) && raw->GetSelection().caret > 0, "page down advances through viewport");
    Require(raw->OnKeyDown(window.Host(), VK_PRIOR, 0) && raw->GetSelection().caret == 0, "page up returns toward document start");
}

void TestRichTextEditorClipboardAtomicityAndSourceContract()
{
    AttachedHostWindow window;
    RichEditorClipboard clipboard;
    auto editor = std::make_unique<RichTextEditor>(L"**\u03b2**\r\n");
    auto* raw   = editor.get();
    window.Host().SetRoot(std::move(editor));
    raw->SetSelection({0, 7});
    clipboard.busy = true;
    Require(raw->OnKeyDown(window.Host(), 'X', MK_CONTROL) && raw->GetText() == L"**\u03b2**\r\n" && ! raw->CanUndo(), "busy clipboard never cuts source");
    clipboard.busy = false;
    Require(raw->OnCopy(window.Host()), "copy through host Unicode clipboard");
    Require(raw->OnKeyDown(window.Host(), 'X', MK_CONTROL) && raw->GetText().empty(), "successful copy precedes cut");
    Require(raw->OnKeyDown(window.Host(), 'V', MK_CONTROL) && raw->GetText() == L"**\u03b2**\r\n", "paste retains exact markers Unicode and EOL");
    raw->SetReadOnly(true);
    raw->SetSelection({0, 7});
    Require(raw->OnCopy(window.Host()), "read-only copy allowed");
    static_cast<void>(raw->OnKeyDown(window.Host(), 'X', MK_CONTROL));
    static_cast<void>(raw->OnKeyDown(window.Host(), 'V', MK_CONTROL));
    Require(raw->GetText() == L"**\u03b2**\r\n", "read-only clipboard mutations blocked");
    raw->SetReadOnly(false);
    const auto revision = raw->Revision();
    Require(window.Host().CopyTextToClipboard(std::wstring(1, wchar_t(0xd800))), "transport fixture can supply malformed UTF-16");
    static_cast<void>(raw->OnKeyDown(window.Host(), 'V', MK_CONTROL));
    Require(raw->Revision() == revision && IsValidUtf16(raw->GetText()), "malformed paste rejected without edit");
    clipboard.text      = L"replacement";
    const auto original = raw->GetText();
    clipboard.afterRead = [&] { raw->SetSelection({1, 1}); };
    static_cast<void>(raw->OnKeyDown(window.Host(), 'V', MK_CONTROL));
    Require(raw->GetText() == original, "clipboard callback changing selection supersedes old paste");
    clipboard.afterRead = [&] { window.Host().SetRoot(std::make_unique<Label>(L"retired")); };
    Require(raw->OnKeyDown(window.Host(), 'V', MK_CONTROL) && dynamic_cast<Label*>(window.Host().GetRoot()),
            "clipboard callback may retire owner without resuming paste");
}

void TestRichTextEditorScrollAndHostClock()
{
    AttachedHostWindow window;
    auto editor = std::make_unique<RichTextEditor>(std::wstring(500, L'\n'));
    auto* raw   = editor.get();
    window.Host().SetRoot(std::move(editor));
    RenderEditor(window);
    Require(raw->OnMouseWheel(window.Host(), {}, -WHEEL_DELTA, 0), "wheel handled");
    RenderEditor(window);
    Require(raw->DebugGetState().scrollOffsetDip > 0, "wheel scrolls bounded viewport");
    Require(! raw->OnMouseWheel(window.Host(), {}, std::numeric_limits<float>::infinity(), 0), "nonfinite wheel rejected");
    raw->SetSelection({500, 500});
    RenderEditor(window);
    const auto bottom = raw->DebugGetState().scrollOffsetDip;
    Require(bottom > 48, "selection reveals end caret");
    raw->SetSelection({0, 0});
    RenderEditor(window);
    Require(raw->DebugGetState().scrollOffsetDip == 0, "selection reveals start caret");
    window.Host().SetFocusControl(raw, false);
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = false;
    window.Host().SetTheme(theme);
    Require(raw->Tick(window.Host(), 0), "host epoch can begin at zero");
    const auto invalidations = window.Host().DebugGetInvalidateCount();
    Require(raw->Tick(window.Host(), 100) && window.Host().DebugGetInvalidateCount() == invalidations, "same caret phase does not dirty pixels");
    Require(raw->Tick(window.Host(), 500) && window.Host().DebugGetInvalidateCount() > invalidations, "phase flip invalidates");
    Require(raw->Tick(window.Host(), 10), "backwards host timestamp resets epoch safely");
    raw->SetReadOnly(true);
    Require(! raw->Tick(window.Host(), 1000), "read-only caret requests no periodic work");
}

void TestRichTextEditorCatalogAndRebinding()
{
    std::unique_ptr<Control> control;
    RequireSucceeded(CreateControl(ControlKind::RichTextEditor, control), "catalog creates rich-text editor");
    Require(dynamic_cast<RichTextEditor*>(control.get()) != nullptr, "factory returns retained concrete type");
    bool found = false;
    for (const auto& descriptor : GetControlCatalog())
        found = found || (descriptor.kind == ControlKind::RichTextEditor && descriptor.name == L"RichTextEditor");
    Require(found, "catalog names new control");
    ControlHost first, second;
    auto root    = std::make_unique<Panel>();
    auto* panel  = root.get();
    auto* editor = root->AddChild<RichTextEditor>();
    editor->SetText(L"kept");
    editor->SetSelection({4, 4});
    first.SetRoot(std::move(root));
    Require(editor->ReplaceSelection(L"!"), "source edit in first host");
    // Supported extraction preserves the same identity and history.
    control = panel->TakeChild(0);
    second.SetRoot(std::move(control));
    Require(editor->GetText() == L"kept!" && editor->Undo() && editor->GetText() == L"kept", "host rebinding preserves logical state");
}

void TestRichTextEditorRetainedPreparationFixedTrials()
{
    // Kept byte-identical across the geometry baseline/candidate. This includes
    // the real native host's dirty preparation/raster path, without activation.
    // Five trials describe this process; they are not frame-gate acceptance.
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line)
        seed += L"line **bold** a\u0301 \U0001f600 text\r\n";
    for (size_t trial = 0u; trial < 5u; ++trial)
    {
        AttachedHostWindow window;
        auto editor              = std::make_unique<RichTextEditor>(seed);
        auto* raw                = editor.get();
        const auto firstRevision = raw->Revision();
        window.Host().SetRoot(std::move(editor));
        const auto start = std::chrono::steady_clock::now();
        RenderEditor(window);
        const auto initial     = std::chrono::steady_clock::now();
        constexpr size_t edits = 128u;
        for (size_t edit = 0u; edit < edits; ++edit)
        {
            raw->SetSelection({seed.size() / 2u, seed.size() / 2u + 1u});
            Require(raw->ReplaceSelection(edit % 2u ? L"x" : L"y"), "retained trial replaces one ASCII source unit");
            RenderEditor(window);
        }
        const auto finish = std::chrono::steady_clock::now();
        Require(raw->DebugGetState().layoutReady && raw->GetText().size() == seed.size() && raw->Revision() == firstRevision + edits,
                "retained trial publishes every edited revision with unchanged source length");
        std::cout << "rich retained preparation trial=" << trial << " sourceUnits=" << seed.size() << " edits=" << edits
                  << " initialMs=" << std::chrono::duration<double, std::milli>(initial - start).count()
                  << " editMs=" << std::chrono::duration<double, std::milli>(finish - initial).count() << '\n';
    }
}
void TestRichTextEditorNativeCompatibilityWorkIsReported()
{
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line)
        seed += L"line **bold** a\u0301 \U0001f600 text\r\n";
    AttachedHostWindow window;
    auto editor = std::make_unique<RichTextEditor>(seed);
    auto* raw   = editor.get();
    window.Host().SetRoot(std::move(editor));
    RenderEditor(window);
    raw->SetSelection({seed.size() / 2u, seed.size() / 2u + 1u});
    Require(raw->ReplaceSelection(L"x"), "compatibility work fixture changes actual display text");
    RenderEditor(window);
    const auto state = raw->DebugGetState();
    auto* format     = window.Host().GetTextFormat(FontRole::Body);
    std::cout << "rich native compatibility sourceUnits=" << seed.size() << " dpi=" << window.Host().GetDpi()
              << " bodyFontSize=" << (format ? format->GetFontSize() : 0.0f) << " chunks=" << state.retainedGeometryChunks
              << " wholeGeometry=" << state.wholeGeometryFallback << " wholeRaster=" << state.wholeRasterFallback
              << " preparedChunkSourceUnits=" << state.preparedSourceUnits << " wholeLayoutsPrepared=" << state.wholeLayoutsPrepared
              << " wholeLayoutSourceUnits=" << state.wholeLayoutSourceUnits << '\n';
    Require(state.layoutReady && state.retainedGeometryChunks <= 128u, "native compatibility resource graph remains coherent and bounded");
    Require(! state.wholeGeometryFallback && ! state.wholeRasterFallback && state.reusedGeometryChunks > 0u && state.preparedSourceUnits <= 2048u,
            "the actual default 13-DIP native editor reuses positioned chunks without whole-layout preparation or raster fallback");
    if (state.wholeGeometryFallback || state.wholeRasterFallback)
        Require(state.wholeLayoutsPrepared > 0u && state.wholeLayoutSourceUnits == seed.size(),
                "whole-layout compatibility work is explicitly reported rather than attributed to bounded chunk preparation");
    else
        Require(state.reusedGeometryChunks > 0u && state.preparedSourceUnits <= 2048u, "native exact-placement path reports actual incremental geometry reuse");
}
void TestRichTextEditorNativeChunkReuseAndRetirement()
{
    std::wstring seed;
    for (size_t line = 0u; line < 512u; ++line)
        seed += L"line **bold** a\u0301 \U0001f600\r\n";
    AttachedHostWindow window;
    auto editor = std::make_unique<RichTextEditor>(seed);
    auto* raw   = editor.get();
    window.Host().SetRoot(std::move(editor));
    RenderEditor(window);
    const auto initial = raw->DebugGetState();
    Require(initial.retainedGeometryChunks > 1u && initial.retainedGeometryChunks <= 128u && initial.preparedSourceUnits == seed.size(),
            "native control prepares an actually bounded chunk graph");
    raw->SetSelection({seed.size() / 2u, seed.size() / 2u + 1u});
    Require(raw->ReplaceSelection(L"x") && ! raw->DebugGetState().layoutReady, "native source edit immediately invalidates old geometry");
    RenderEditor(window);
    const auto edited = raw->DebugGetState();
    Require(edited.layoutReady && edited.reusedGeometryChunks > 0u && edited.preparedSourceUnits <= 2048u,
            "native host preparation actually reuses unchanged DirectWrite chunks");
    Require(raw->Undo(), "native retained chunk undo");
    RenderEditor(window);
    Require(raw->GetText() == seed && raw->DebugGetState().reusedGeometryChunks > 0u, "undo reuses matching native geometry and restores exact source");
    raw->SetVisible(false);
    RenderEditor(window);
    Require(! raw->DebugGetState().layoutReady && raw->DebugGetState().retainedGeometryChunks == 0u, "hiding native control retires the chunk graph");
    raw->SetVisible(true);
    RenderEditor(window);
    Require(raw->DebugGetState().layoutReady && raw->DebugGetState().reusedGeometryChunks == 0u, "show rebuilds retired native resources");
    raw->SetViewMode(ViewMode::Source);
    RenderEditor(window);
    Require(raw->DebugGetState().reusedGeometryChunks == 0u && raw->GetText() == seed, "source mode retires rich geometry without rewriting source");
}
void TestRichTextEditorNativeCooperativePreparation()
{
    std::wstring seed;
    for (size_t line = 0u; line < 1024u; ++line)
        seed += L"line **bold** a\u0301 \U0001f600 text\r\n";
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>(seed);
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    Require(window.Host().RenderInitialFrameForShow(), "one native owner preparation turn");
    const auto pending = editor->DebugGetState();
    Require(pending.preparationPending && ! pending.layoutReady && window.Host().HasPendingContentPreparation() && pending.lastAdvanceLayouts <= 4u &&
                pending.lastAdvanceBlockSteps <= 4096u,
            "large native source yields after one bounded owner advance");
    Require(! editor->TryGetTextInputCaretRect(window.Host(), 0u) && ! editor->TryGetTextInputRangeRects(window.Host(), 0u, 4u) &&
                ! editor->GetVisibleSourceRange(window.Host()) && ! editor->GetSourceVisualLineRanges(window.Host()),
            "native text/UIA geometry is unavailable during pending preparation");
    Require(! editor->ScrollSourceRangeIntoView(window.Host(), 0u, 4u, true) && editor->DebugGetState().preparationAdvances == pending.preparationAdvances,
            "queries and scrolling never drain a pending native request");
    for (size_t key = 0u; key < 48u; ++key)
        Require(editor->OnChar(window.Host(), L'x', 0u), "rapid typing stays accepted before native geometry catches up");
    const auto latest = editor->GetText();
    Require(editor->DebugGetState().preparationAdvances == pending.preparationAdvances && editor->DebugGetState().cancelledPreparations > 0u,
            "rapid source edits coalesce without advancing superseded glyph work");
    Require(editor->OnKeyDown(window.Host(), VK_BACK, 0u) && editor->Undo() && editor->GetText() == latest,
            "pending source deletion remains a complete Unicode undo transaction");
    RenderEditor(window);
    Require(editor->DebugGetState().layoutReady && editor->GetText() == latest && ! window.Host().HasPendingContentPreparation(),
            "native publication contains the latest source, never an obsolete staged revision");
    editor->SetText(seed);
    Require(window.Host().RenderInitialFrameForShow() && editor->DebugGetState().preparationPending, "enter pending native typography fixture");
    editor->SetDensity(Density::Compact);
    Require(! editor->DebugGetState().preparationPending && editor->DebugGetState().retainedGeometryChunks == 0u,
            "density cancels work using previous text viewport options");
    Require(window.Host().RenderInitialFrameForShow() && editor->DebugGetState().preparationPending, "begin pending native direction fixture");
    editor->SetFlowDirection(FlowDirection::RightToLeft);
    Require(! editor->DebugGetState().preparationPending, "direction cancels the previous shaping request");
    Require(window.Host().RenderInitialFrameForShow() && editor->DebugGetState().preparationPending, "begin pending native resource fixture");
    const auto resourceGeneration = window.Host().GetResourceGeneration();
    window.Host().DebugSimulateDeviceLoss();
    Require(window.Host().GetResourceGeneration() != resourceGeneration && ! editor->DebugGetState().preparationPending &&
                editor->DebugGetState().retainedGeometryChunks == 0u && editor->GetText() == seed,
            "native device loss rejects pending resource identities without losing source");
    Require(window.Host().RenderInitialFrameForShow() && editor->DebugGetState().preparationPending, "native cancellation fixture enters pending work");
    editor->SetViewMode(ViewMode::Source);
    Require(! editor->DebugGetState().preparationPending && editor->DebugGetState().retainedGeometryChunks == 0u,
            "mode replacement immediately retires staged and published layouts");
    Require(window.Host().RenderInitialFrameForShow(), "begin replacement native mode");
    editor->SetVisible(false);
    Require(! editor->DebugGetState().preparationPending && editor->GetText() == seed, "hiding cancels native staging and preserves source");
    RenderEditor(window);
    editor->SetVisible(true);
    RenderEditor(window);
    const auto complete = editor->DebugGetState();
    RenderEditor(window);
    Require(editor->DebugGetState().preparationAdvances == complete.preparationAdvances &&
                editor->DebugGetState().layoutGeneration == complete.layoutGeneration,
            "clean native repaint does not resume or rebuild completed work");
}
void TestRichTextEditorNativeMessageSchedulingAndFailure()
{
    class FailingEditor final : public RichTextEditor
    {
    public:
        FailingEditor() : RichTextEditor(L"retained source")
        {
        }
        bool fail = true;
        size_t calls{};
        HRESULT PrepareContent(ControlHost& host) noexcept override
        {
            ++calls;
            return fail ? E_OUTOFMEMORY : RichTextEditor::PrepareContent(host);
        }
    };
    AttachedHostWindow window;
    auto owned   = std::make_unique<FailingEditor>();
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    Require((GetWindowLongPtrW(window.Hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE) != 0, "native scheduler fixture cannot activate the person's desktop window");
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    const auto waitFor = [&](auto predicate)
    {
        const auto deadline = GetTickCount64() + 12000u;
        while (! predicate() && GetTickCount64() < deadline)
        {
            window.PumpMessages(10u);
            Sleep(1u);
        }
        return predicate();
    };
    Require(waitFor([&] { return editor->calls > 0u; }), "ordinary WM_PAINT reaches shared content preparation");
    window.PumpMessages(20u); // Consume any initial-show messages before observing failed-work idleness.
    const auto failedCalls = editor->calls;
    const auto idleUntil   = GetTickCount64() + 40u;
    while (GetTickCount64() < idleUntil)
    {
        window.PumpMessages(10u);
        Sleep(1u);
    }
    Require(editor->calls == failedCalls && window.Host().HasPendingContentPreparation() && ! editor->DebugGetState().layoutReady,
            "failed native work stays unavailable without a self-invalidating repaint loop");
    std::wstring source;
    for (size_t line = 0u; line < 1024u; ++line)
        source += L"line **bold** a\u0301 \U0001f600 text\r\n";
    editor->fail = false;
    editor->SetText(source);
    Require(waitFor([&] { return ! window.Host().HasPendingContentPreparation(); }),
            "native invalidation schedules every cooperative continuation through WM_PAINT");
    const auto prepared = editor->DebugGetState();
    Require(prepared.layoutReady && prepared.preparationAdvances > 1u && editor->GetText() == source,
            "multiple message-loop turns publish only the latest complete native document");
    window.PumpMessages(20u);
    Require(editor->DebugGetState().preparationAdvances == prepared.preparationAdvances, "completed native content leaves no preparation polling loop");
}
void TestRichTextEditorNativeDenseSourceAndSplitScheduling()
{
    class ObservedEditor final : public RichTextEditor
    {
    public:
        using RichTextEditor::RichTextEditor;
        size_t calls{};
        std::chrono::microseconds maximumAdvance{};
        HRESULT PrepareContent(ControlHost& host) noexcept override
        {
            ++calls;
            const auto start = std::chrono::steady_clock::now();
            const HRESULT hr = RichTextEditor::PrepareContent(host);
            maximumAdvance   = std::max(maximumAdvance, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start));
            return hr;
        }
    };
    for (const size_t blockCount : {1500u, 6000u})
    {
        std::wstring source;
        for (size_t block = 0u; block < blockCount; ++block)
            source.append(L"```\nx\n```\n");
        AttachedHostWindow window;
        auto root        = std::make_unique<Panel>();
        auto* sourceView = root->AddChild<ObservedEditor>(source);
        auto* richView   = root->AddChild<ObservedEditor>(source);
        sourceView->SetBounds(D2D1::RectF(0.f, 0.f, 145.f, 145.f));
        richView->SetBounds(D2D1::RectF(150.f, 0.f, 295.f, 145.f));
        sourceView->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
        sourceView->SetViewMode(ViewMode::Source);
        auto appearance                   = sourceView->GetAppearance();
        appearance.colorizeMarkdownSource = true;
        sourceView->SetAppearance(appearance);
        richView->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
        richView->SetMarkdownRenderingEnabled(true);
        window.Host().SetRoot(std::move(root));
        Require((GetWindowLongPtrW(window.Hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE) != 0, "dense split fixture cannot activate the person's desktop window");
        const auto start    = std::chrono::steady_clock::now();
        const auto deadline = GetTickCount64() + 12000u;
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        while (window.Host().HasPendingContentPreparation() && GetTickCount64() < deadline)
        {
            window.PumpMessages(10u);
            Sleep(1u); // A failed/stalled request must not turn the test's independent deadline into a busy loop.
        }
        const auto elapsed        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        const auto preparedSource = sourceView->DebugGetState(), preparedRich = richView->DebugGetState();
        std::cout << "    [DENSE-SOURCE] blocks=" << blockCount << " elapsed_ms=" << elapsed.count() << " source_turns=" << sourceView->calls
                  << " rich_turns=" << richView->calls << " source_max_us=" << sourceView->maximumAdvance.count()
                  << " rich_max_us=" << richView->maximumAdvance.count() << " ink_ranges=" << preparedSource.sourceInkRangeCount << '\n';
        Require(! window.Host().HasPendingContentPreparation() && preparedSource.layoutReady && preparedRich.layoutReady &&
                    preparedSource.sourceInkRangeCount >= blockCount && ! preparedSource.sourceInkPreparationPending &&
                    richView->GetMarkdownRenderingStatus() ==
                        (blockCount < SemanticDocument::MaximumParagraphs ? MarkdownRenderingStatus::Semantic : MarkdownRenderingStatus::SourceFallback),
                "ordinary native messages finish dense Source ink and sibling Rich geometry within the independent deadline");
        Require(sourceView->GetText() == source && richView->GetText() == source && ! sourceView->CanUndo() && ! richView->CanUndo() &&
                    sourceView->GetSelection() == Selection{} && richView->GetSelection() == Selection{},
                "dense cooperative preparation preserves both exact source bindings, selection and histories");
        const auto sourceCalls = sourceView->calls, richCalls = richView->calls;
        window.PumpMessages(20u);
        Require(sourceView->calls == sourceCalls && richView->calls == richCalls, "completed dense split preparation leaves no polling work");
    }
}
void TestRichTextEditorNativeHostVisibilityAndZeroHeight()
{
    std::wstring source;
    for (size_t line = 0u; line < 1024u; ++line)
        source += L"line **bold** a\u0301 \U0001f600 text\r\n";
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>(source);
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    Require((GetWindowLongPtrW(window.Hwnd(), GWL_EXSTYLE) & WS_EX_NOACTIVATE) != 0, "native suspension fixture cannot activate the person's desktop window");
    editor->SetSelection({3u, 3u});
    Require(editor->ReplaceSelection(L"x"), "native suspension fixture retains an undo transaction");
    source               = editor->GetText();
    const auto selection = editor->GetSelection();
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    Require(window.Host().RenderInitialFrameForShow() && editor->DebugGetState().preparationPending,
            "native window hide fixture has staged work before suspension");
    ShowWindow(window.Hwnd(), SW_HIDE);
    const auto hidden = editor->DebugGetState();
    Require(! hidden.preparationPending && ! hidden.layoutReady && hidden.retainedGeometryChunks == 0u,
            "native window hide retires both staged and published content without requiring control visibility changes");
    window.PumpMessages(20u);
    Require(editor->DebugGetState().preparationAdvances == hidden.preparationAdvances && editor->GetText() == source && editor->GetSelection() == selection &&
                editor->CanUndo(),
            "hidden native window does not resume glyph work or lose source, selection or history");
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    Require(window.Host().RenderInitialFrameForShow() && editor->DebugGetState().preparationPending, "show starts a fresh bounded native request");
    RECT client{};
    Require(GetClientRect(window.Hwnd(), &client) != FALSE && client.right > 0 && client.bottom > 0,
            "native suspension fixture has a positive original client size");
    // Send the host's ordinary resize protocol with unchanged width and zero
    // height. This specifically catches relying only on shaping-width changes.
    SendMessageW(window.Hwnd(), WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right, 0));
    Require(! editor->DebugGetState().preparationPending && ! editor->DebugGetState().layoutReady && editor->DebugGetState().retainedGeometryChunks == 0u &&
                ! window.Host().HasActiveNativeTextInputSession(),
            "native height-only zero extent explicitly cancels preparation and text-service geometry");
    SendMessageW(window.Hwnd(), WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right, client.bottom));
    RenderEditor(window);
    Require(editor->DebugGetState().layoutReady && editor->GetText() == source && editor->GetSelection() == selection && editor->CanUndo(),
            "native restore publishes current geometry and preserves the edit transaction");
}
void TestRichTextEditorNativePreparationCanReplaceRoot()
{
    class ReplacingEditor final : public RichTextEditor
    {
    public:
        ReplacingEditor() : RichTextEditor(std::wstring(8192u, L'x')), replacement(std::make_unique<Label>(L"current root"))
        {
        }
        std::unique_ptr<Control> replacement;
        HRESULT PrepareContent(ControlHost& host) noexcept override
        {
            const auto result = RichTextEditor::PrepareContent(host);
            if (result == S_OK && replacement)
            {
                try
                {
                    host.SetRoot(std::move(replacement));
                }
                catch (const std::exception&)
                {
                    return E_FAIL;
                }
                return S_FALSE; // this has retired; do not read another member.
            }
            return result;
        }
    };
    AttachedHostWindow window;
    auto editor         = std::make_unique<ReplacingEditor>();
    const auto lifetime = GetControlLifetimeToken(*editor);
    window.Host().SetRoot(std::move(editor));
    Require(window.Host().RenderInitialFrameForShow() && lifetime.expired(), "native shared traversal tolerates root retirement from preparation");
    Require(dynamic_cast<Label*>(window.Host().GetRoot()) != nullptr && window.Host().RenderInitialFrameForShow(),
            "replacement root prepares without publishing retired editor work");
}
} // namespace

namespace
{
void TestRichTextEditorNativeBlockGrammarAndIdentity()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::CommonMark0312);
    for (const auto& scenario : MarkdownBlockTests::scenarios)
    {
        editor->SetText(std::wstring(scenario.source));
        RenderEditor(window);
        const auto syntax = editor->GetMarkdownSyntax();
        Require(syntax && MarkdownBlockTests::BlockSignature(*syntax) == scenario.signature, "native block grammar scenario matches its independent structure");
        Require(editor->GetText() == scenario.source && ! editor->CanUndo(), "native block analysis preserves source and history");
    }
    editor->SetText(L"# heading\n\n> alpha\n\ntail\n");
    RenderEditor(window);
    const auto old  = editor->GetMarkdownSyntax();
    const auto text = editor->GetText();
    const auto at   = text.find(L"alpha");
    editor->SetSelection({at, at});
    Require(editor->ReplaceSelection(L"\U0001f600 "), "native structural source edit");
    Require(! editor->GetMarkdownSyntax(), "native edit retires current syntax immediately");
    RenderEditor(window);
    const auto current = editor->GetMarkdownSyntax();
    Require(current && current->DocumentIdentity() == old->DocumentIdentity() && current->Nodes().size() == old->Nodes().size(),
            "native identity scope survives owner scheduling");
    for (std::size_t i = 0; i < old->Nodes().size(); ++i)
        Require(current->Nodes()[i].identity == old->Nodes()[i].identity, "native mapped nodes retain identity");
    Require(editor->Undo(), "native block edit undo");
    RenderEditor(window);
    Require(editor->GetText() == text && old->Source() == text, "native undo and held syntax retain exact source");
    editor->SetText(L"same\n\nsame\n\nsame\n");
    RenderEditor(window);
    const auto duplicates = editor->GetMarkdownSyntax();
    const auto first      = duplicates->Nodes()[0u].firstChild;
    const auto second     = duplicates->Nodes()[first].nextSibling;
    editor->SetSelection({0u, 6u});
    Require(editor->ReplaceSelection(L""), "native delete first duplicate");
    RenderEditor(window);
    const auto remaining = editor->GetMarkdownSyntax();
    Require(remaining->Nodes()[remaining->Nodes()[0u].firstChild].identity == duplicates->Nodes()[second].identity,
            "native tracked source ancestry preserves the surviving duplicate rather than its equal predecessor");
    std::wstring nested;
    for (unsigned level = 0u; level < 256u; ++level)
        nested += L"- ";
    nested += L"alpha\n";
    nested.append(512u, L' ');
    nested += L"beta\n";
    editor->SetText(nested);
    RenderEditor(window);
    const auto bounded = editor->GetMarkdownSyntax();
    Require(bounded && bounded->Nodes().size() == 514u && bounded->BlockStatistics().prefixInspectionUnits <= nested.size() * 40u,
            "native owner preparation preserves deep list structure with bounded prefix inspection");
    for (const auto& edge : MarkdownBlockTests::identityEdges)
    {
        editor->SetText(std::wstring(edge.source));
        RenderEditor(window);
        const auto before = editor->GetMarkdownSyntax();
        editor->SetSelection(edge.selection);
        Require(editor->ReplaceSelection(L""), "native Unicode paragraph-edge deletion");
        RenderEditor(window);
        const auto after = editor->GetMarkdownSyntax();
        Require(after && after->Nodes()[after->Nodes()[0u].firstChild].identity == before->Nodes()[before->Nodes()[0u].firstChild].identity,
                "native first/last scalar deletion retains the compatible paragraph identity");
    }
}
void TestRichTextEditorNativeInlineGrammarAndReferenceEdits()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        editor->SetMarkdownAnalysisProfile(profile);
        for (const auto& scenario : MarkdownInlineTests::scenarios)
        {
            editor->SetText(std::wstring(scenario.source));
            RenderEditor(window);
            const auto syntax = editor->GetMarkdownSyntax();
            Require(syntax && MarkdownHtmlTests::Html(*syntax) == MarkdownInlineTests::Expected(scenario, profile),
                    "native inline/extension scenario matches independent expected semantics");
            Require(editor->GetText() == scenario.source && ! editor->CanUndo(), "native inline analysis preserves exact source and history");
        }
    }
    editor->SetText(std::wstring(MarkdownInlineTests::paddedTable));
    RenderEditor(window);
    const auto table = editor->GetMarkdownSyntax();
    const auto cells = MarkdownInlineTests::TableCellIdentities(*table);
    Require(cells.size() == 6u, "native three-column table contains both missing body cells");
    editor->SetSelection({editor->GetText().size(), editor->GetText().size()});
    Require(editor->ReplaceSelection(L"outside\n"), "native edit outside padded table");
    RenderEditor(window);
    Require(MarkdownInlineTests::UniqueNodeIdentities(*editor->GetMarkdownSyntax()) &&
                MarkdownInlineTests::TableCellIdentities(*editor->GetMarkdownSyntax()) == cells,
            "native unrelated edit preserves unique source-backed and padded cell identities");
    Require(editor->Undo(), "native padded table unrelated edit undo");
    RenderEditor(window);
    Require(MarkdownInlineTests::UniqueNodeIdentities(*editor->GetMarkdownSyntax()) &&
                MarkdownInlineTests::TableCellIdentities(*editor->GetMarkdownSyntax()) == cells,
            "native undo retains distinct padded cell identities");
    Require(editor->Redo(), "native padded table unrelated edit redo");
    RenderEditor(window);
    Require(MarkdownInlineTests::UniqueNodeIdentities(*editor->GetMarkdownSyntax()) &&
                MarkdownInlineTests::TableCellIdentities(*editor->GetMarkdownSyntax()) == cells && MarkdownInlineTests::UniqueNodeIdentities(*table),
            "native redo and held table snapshots retain distinct padded cell identities");
    editor->SetText(L"[reader]\n\n[reader]: /first\n");
    RenderEditor(window);
    const auto before            = editor->GetMarkdownSyntax();
    const auto paragraphIdentity = before->Nodes()[before->Nodes()[0u].firstChild].identity;
    const auto at                = editor->GetText().find(L"/first");
    editor->SetSelection({at, at + 6u});
    Require(editor->ReplaceSelection(L"/second"), "native reference destination edit");
    Require(! editor->GetMarkdownSyntax(), "reference edits immediately retire stale inline semantics");
    RenderEditor(window);
    const auto after = editor->GetMarkdownSyntax();
    Require(MarkdownHtmlTests::Html(*after) == L"<p><a href=\"/second\">reader</a></p>\n" &&
                after->Nodes()[after->Nodes()[0u].firstChild].identity == paragraphIdentity,
            "unchanged paragraph identity cannot validate stale reference semantics");
    Require(MarkdownHtmlTests::Html(*before) == L"<p><a href=\"/first\">reader</a></p>\n", "held native inline tree remains immutable");
    Require(editor->Undo(), "native reference undo");
    RenderEditor(window);
    Require(MarkdownHtmlTests::Html(*editor->GetMarkdownSyntax()) == MarkdownHtmlTests::Html(*before), "native undo re-resolves global references");
    Require(editor->Redo(), "native reference redo");
    RenderEditor(window);
    Require(MarkdownHtmlTests::Html(*editor->GetMarkdownSyntax()) == MarkdownHtmlTests::Html(*after), "native redo re-resolves global references");
}
void TestRichTextEditorNativeFrontMatterFencesAndSourceEdits()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    window.Host().SetRoot(std::move(owned));
    for (const auto& scenario : FrontMatterTests::scenarios)
    {
        editor->SetText(std::wstring(scenario.source));
        RenderEditor(window);
        const auto syntax = editor->GetMarkdownSyntax();
        Require(syntax != nullptr, "native metadata source preparation completes");
        FrontMatterTests::CheckSyntax(*syntax, scenario, Require);
        const auto semantic = editor->GetAcknowledgedSemanticDocument(window.Host());
        Require(semantic && semantic->Syntax() == syntax && editor->GetText() == scenario.source && ! editor->CanUndo(),
                "native metadata shares acknowledged syntax and preserves exact source/history");
    }
    const std::wstring source = L"+++\r\n# keep comment\r\ntitle = '\u03b2'\r\n+++\r\n# Body\r\n";
    editor->SetText(source);
    RenderEditor(window);
    const auto held = editor->GetAcknowledgedSemanticDocument(window.Host());
    const auto at   = source.find(L'\u03b2');
    editor->SetSelection({at, at + 1u});
    for (const auto command : {MarkdownCommand::ToggleStyle, MarkdownCommand::SetHeading, MarkdownCommand::InsertTable})
        Require(editor->ExecuteMarkdownCommand({command}) == MarkdownCommandStatus::Unavailable && editor->GetText() == source && ! editor->CanUndo(),
                "Markdown structure commands cannot rewrite an opaque metadata value");
    Require(editor->ReplaceSelection(L"\u03a9") && ! editor->GetAcknowledgedSemanticDocument(window.Host()),
            "native source edit invalidates old metadata acknowledgement synchronously");
    auto expected = source;
    expected.replace(at, 1u, L"\u03a9");
    RenderEditor(window);
    Require(editor->GetText() == expected && held->Syntax()->Source() == source, "native metadata edit preserves untouched spelling and held source");
    Require(editor->Undo() && editor->GetText() == source && ! editor->CanUndo() && editor->Redo() && editor->GetText() == expected,
            "native metadata replacement is one reversible source transaction");
    const auto selected = editor->GetSelection();
    editor->SetViewMode(ViewMode::Source);
    RenderEditor(window);
    Require(editor->GetText() == expected && editor->GetSelection() == selected, "native source view preserves metadata selection and exact document");
}
void TestRichTextEditorNativeFlowchartRenderingAndFallbacks()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    window.Host().SetRoot(std::move(owned));
    Require(SetWindowPos(window.Hwnd(), nullptr, 0, 0, 1024, 300, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE, "wide native diagram corpus host");
    window.PumpMessages();
    for (const auto& scenario : NativeFlowchartTests::scenarios)
    {
        const auto source = NativeFlowchartTests::FencedSource(scenario);
        editor->SetText(source);
        RenderEditor(window);
        const auto syntax = editor->GetMarkdownSyntax();
        Require(syntax != nullptr, "native Mermaid source preparation completes");
        NativeFlowchartTests::CheckFencedSource(*syntax, scenario, Require);
        const auto semantic = editor->GetAcknowledgedSemanticDocument(window.Host());
        Require(semantic && semantic->Syntax() == syntax && semantic->Map().Source() == source && editor->GetText() == source && ! editor->CanUndo(),
                "native graph rendering/fallback retains exact definition and history");
        Require(scenario.outcome == RichText::Detail::FlowchartOutcome::Prepared ? ! RichText::Detail::SemanticExtensionAccess::Diagrams(*semantic).empty()
                                                                                 : semantic->Map().Display() == NativeFlowchartTests::LiteralDisplay(scenario),
                "admitted native graphs render atomically while rejected definitions retain code presentation");
        editor->SetSelection({13u, 13u});
        Require(editor->ReplaceSelection(L"\u03b2") && editor->Undo() && editor->GetText() == source,
                "every accepted/rejected native diagram definition remains a reversible Unicode source edit");
    }
}
void TestRichTextEditorNativeDiagramViewCapacityRecovery()
{
    AttachedHostWindow window;
    Require(SetWindowPos(window.Hwnd(), nullptr, 0, 0, 640, 300, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE, "native diagram fixture outer window");
    auto root    = std::make_unique<Panel>();
    auto* editor = root->AddChild<RichTextEditor>();
    window.Host().SetRoot(std::move(root));
    UINT lastWidth{};
    SemanticDiagramTests::Editor(*editor,
                                 window.Host(),
                                 [&](UINT width)
    {
        if (width != lastWidth)
        {
            // Size the hosted editor in DIP, independently of top-level window
            // tracking/nonclient/DPI constraints. Both hosts receive the same
            // actual text viewport widths, including the narrow rejection.
            editor->SetBounds(D2D1::RectF(0.f, 0.f, static_cast<float>(width), 240.f));
            lastWidth = width;
            window.PumpMessages();
        }
        RenderEditor(window);
    },
                                 Require);
    RichEditorClipboard clipboard;
    const auto source = editor->GetText();
    editor->SetSelection({0u, source.size()});
    Require(editor->OnCopy(window.Host()) && clipboard.text == source,
            "native graphic selection copies the exact source fence rather than its generated caption");
    Require(editor->OnKeyDown(window.Host(), 'X', MK_CONTROL) && editor->GetText().empty() && clipboard.text == source && editor->Undo() &&
                editor->GetText() == source,
            "native graphic source cut is one reversible Unicode editing transaction");
    RenderEditor(window);
    Require(editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic, "undo of source cut restores the native graphic");
}
void TestRichTextEditorNativeDiagramSelectionRaster()
{
    AttachedHostWindow window;
    window.Host().SetTheme(SemanticDiagramTests::SelectionPalette());
    auto owned   = std::make_unique<RichTextEditor>(std::wstring(SemanticDiagramTests::SelectionSource));
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    window.Host().SetRoot(std::move(owned));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    RenderEditor(window);
    editor->SetSelection({0u, editor->GetText().size()});
    RenderEditor(window);
    WindowHostBitmapCapture capture;
    Require(window.Host().DebugCaptureBitmap(capture), "native selected graphic capture without activating the desktop");
    SemanticDiagramTests::SelectionPixels(
        *editor, window.Host(), capture.bgraPixels, capture.widthPx, capture.heightPx, window.Host().GetDpi() / 96.f, Require);
}
void TestRichTextEditorNativeFootnoteBlockSourceEdits()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    window.Host().SetRoot(std::move(owned));
    for (const auto& scenario : FootnoteBlockTests::scenarios)
    {
        editor->SetText(std::wstring(scenario.source));
        RenderEditor(window);
        const auto syntax = editor->GetMarkdownSyntax();
        Require(syntax != nullptr, "native note blocks prepare on the host owner turns");
        FootnoteBlockTests::CheckSyntax(*syntax, scenario, Require);
        const auto semantic = editor->GetAcknowledgedSemanticDocument(window.Host());
        Require(semantic && semantic->Syntax() == syntax && editor->GetText() == scenario.source && ! editor->CanUndo(),
                "native note source and history survive literal semantic preparation");
    }
    const std::wstring source = L"[^n]: first\r\n\r\n    second\r\n";
    editor->SetText(source);
    RenderEditor(window);
    const auto at = source.find(L"second");
    editor->SetSelection({at, at + 6u});
    for (const auto command : {MarkdownCommand::ToggleStyle, MarkdownCommand::SetHeading, MarkdownCommand::SetList, MarkdownCommand::InsertTable})
        Require(editor->ExecuteMarkdownCommand({command}) == MarkdownCommandStatus::Unavailable && editor->GetText() == source && ! editor->CanUndo(),
                "initial native literal note commands reject child prefix reinterpretation atomically");
    Require(editor->ReplaceSelection(L"\u03b2") && editor->Undo() && editor->GetText() == source && editor->Redo(),
            "raw native note replacement remains one reversible Unicode transaction");
}
void TestRichTextEditorNativeFootnoteResolutionAndSourceEdits()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    window.Host().SetRoot(std::move(owned));
    FootnoteResolutionTests::ExerciseSourceEdits(*editor,
                                                 [&] { RenderEditor(window); },
                                                 [&](const auto& syntax)
    {
        const auto semantic = editor->GetAcknowledgedSemanticDocument(window.Host());
        if (semantic)
            FootnoteResolutionTests::CheckSemantic(*semantic, Require);
        return semantic && semantic->Syntax() == syntax && semantic->Map().Source() == syntax->Source();
    },
                                                 Require);
    FootnoteResolutionTests::ExerciseNavigation(*editor, window.Host(), [&] { RenderEditor(window); }, Require, MK_CONTROL, MK_SHIFT);
    RichEditorClipboard clipboard;
    const auto source = editor->GetText();
    const auto first = source.find(L"A body") + 1u, last = source.find(L"main") + 1u;
    editor->SetSelection({last, first});
    const auto redo = editor->CanRedo();
    Require(editor->OnCopy(window.Host()) && clipboard.text == source.substr(first, last - first) && editor->CanRedo() == redo && editor->GetText() == source,
            "copy across reordered footers uses the exact raw source interval without touching history or the system clipboard");
}
void TestRichTextEditorNativeParserCapacitySourceFallback()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::Gfm029);
    editor->SetMarkdownRenderingEnabled(true);
    window.Host().SetRoot(std::move(owned));
    MarkdownCapacityTests::EditorFallback(*editor, window.Host(), [&] { RenderEditor(window); }, Require);
}
void TestRichTextEditorNativeRetainedObjectSourceEditing()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<SemanticSourceBlockTests::SourceEditingEditor>();
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    SemanticSourceBlockTests::Retained(*editor, window.Host(), [&] { RenderEditor(window); }, Require);
}
void TestRichTextEditorNativeImagePathPaste()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    RichImagePathTests::Paths(Require);
    RichImagePathTests::Codec(Require);
    RichImagePathTests::Retained(*editor, [&] { RenderEditor(window); }, Require);
    RichEditorClipboard clipboard;
    editor->SetText(L"ok ");
    editor->SetSelection({3u, 3u});
    RenderEditor(window);
    clipboard.text = L"C:\\docs\\a.png";
    Require(editor->OnKeyDown(window.Host(), 'V', MK_CONTROL) && editor->GetText() == L"ok ![a](a.png)",
            "native clipboard action commits generated source through ordinary history");
    Require(editor->Undo() && editor->GetText() == L"ok ", "native path paste has one undo transaction");
}
void TestRichTextNativeImageDecoder()
{
    RichImageTests::Core(Require);
    RichImageResourceTests::Admission(Require);
}
void TestRichTextEditorNativeFileReferencePaste()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    RichImagePathTests::FileReferences(Require);
    RichImagePathTests::RetainedFileReferences(*editor, [&] { RenderEditor(window); }, Require);
    editor->SetText(L"replace");
    editor->SetSelection({7u, 0u});
    RenderEditor(window);
    ClipboardTransportFixture clipboard(window.Hwnd(), Require);
    constexpr wchar_t list[] = L"C:\\docs\\a.png\0C:\\docs\\report.pdf\0";
    const auto bytes         = RichImagePathTests::DropBuffer(std::wstring_view(list, std::size(list)));
    clipboard.fileData.reset(GlobalAlloc(GMEM_MOVEABLE, bytes.size()));
    Require(clipboard.fileData != nullptr, "native file-reference fixture owns clipboard storage");
    auto* data = GlobalLock(clipboard.fileData.get());
    Require(data != nullptr, "native file-reference fixture locks clipboard storage");
    std::memcpy(data, bytes.data(), bytes.size());
    GlobalUnlock(clipboard.fileData.get());
    std::size_t notices{};
    editor->SetOnTextChanged([&] { ++notices; });
    Require(editor->OnKeyDown(window.Host(), 'V', MK_CONTROL) && editor->GetText() == L"![a](a.png)\r\n\r\n[report\\.pdf](report.pdf)" && notices == 1u,
            "native Ctrl+V reads real wide-file transport and commits mixed Markdown once");
    Require(clipboard.opens == 1u && clipboard.closes == 1u, "native mixed paste uses one nonblocking clipboard read");
    Require(editor->Undo() && editor->GetText() == L"replace" && editor->GetSelection() == Selection{7u, 0u} && ! editor->CanUndo(),
            "native mixed clipboard paste restores the backward selection in one undo");
    editor->SetOnTextChanged({});
    clipboard.fileData.reset();
    RequireSucceeded(SystemTextClipboard().Write(window.Hwnd(), L"\"C:\\docs\\files\\report.pdf\""), "native Unicode file-path clipboard data");
    RenderEditor(window);
    Require(editor->OnKeyDown(window.Host(), 'V', MK_CONTROL) && editor->GetText() == L"[report\\.pdf](files/report.pdf)",
            "native quoted text file path becomes a full-filename relative link");
    Require(editor->Undo() && editor->GetText() == L"replace", "native text file-path link is one undo transaction");
}
void TestRichTextEditorNativeImageResources()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    const auto scale = window.Host().GetDpi() / 96.f;
    Require(SetWindowPos(
                window.Hwnd(), nullptr, 0, 0, static_cast<int>(640.f * scale), static_cast<int>(320.f * scale), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) !=
                FALSE,
            "native image fixture has a stable DIP viewport");
    RichImageResourceTests::Retained(*editor, window.Host(), [&] { RenderEditor(window); }, Require);
    editor->ReloadImages();
    RenderEditor(window);
    std::vector<RichImageRequest> requests(0u);
    editor->TakePendingImageRequests(requests);
    Require(requests.size() == 1u, "native retirement fixture has one pending image");
    const auto retired = requests[0];
    window.Host().SetRoot(nullptr);
    Require(retired.Cancelled(), "native control destruction cancels its externally held image job");
}
void TestRichTextEditorNativeImagePixels()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    const auto scale = window.Host().GetDpi() / 96.f;
    Require(SetWindowPos(
                window.Hwnd(), nullptr, 0, 0, static_cast<int>(640.f * scale), static_cast<int>(320.f * scale), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) !=
                FALSE,
            "native raster fixture has a stable DIP viewport");
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    RichImageResourceTests::ReadyPicture(*editor, [&] { RenderEditor(window); }, Require, true);
    WindowHostBitmapCapture capture;
    Require(window.Host().DebugCaptureBitmap(capture), "native image raster capture without desktop activation");
    RichImageResourceTests::PatternPixels(
        *editor, window.Host(), capture.bgraPixels, capture.widthPx, capture.heightPx, window.Host().GetDpi() / 96.f, Require);
    editor->SetSelection({0u, editor->GetText().size()});
    RenderEditor(window);
    Require(window.Host().DebugCaptureBitmap(capture), "native selected image raster capture");
    RichImageResourceTests::PatternPixels(
        *editor, window.Host(), capture.bgraPixels, capture.widthPx, capture.heightPx, window.Host().GetDpi() / 96.f, Require, true);
}
void TestRichTextEditorNativeLosslessMarkdownPreparation()
{
    class SourceAdapterEditor final : public RichTextEditor
    {
    public:
        using RichTextEditor::ExportTextInputState;
        using RichTextEditor::RichTextEditor;
    };
    const std::wstring source = L"# Unicode \U0001f600\r\n\n~~~lang\r\n# kept &NotEqualTilde;\n~~~\n[^unknown]: $x$\n";
    AttachedHostWindow window;
    auto owned   = std::make_unique<SourceAdapterEditor>(source);
    auto* editor = owned.get();
    window.Host().SetRoot(std::move(owned));
    RenderEditor(window);
    Require(! editor->GetMarkdownAnalysisProfile() && ! editor->GetMarkdownSyntax(), "analysis is additive and opt-in");
    editor->SetSelection({source.size(), source.size()});
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        const auto revision = editor->Revision();
        editor->SetMarkdownAnalysisProfile(profile);
        Require(! editor->GetMarkdownSyntax() && editor->GetText() == source && editor->Revision() == revision && ! editor->CanUndo(),
                "profile selection preserves exact source/selection/history without fabricating a current result");
        RenderEditor(window);
        const auto syntax = editor->GetMarkdownSyntax();
        Require(syntax && syntax->Source() == source && syntax->Profile() == profile && syntax->Blocks()[2].kind == MarkdownBlockKind::FencedCode,
                "native ControlHost publishes keyed root syntax on ordinary owner turns");
        const auto advances = editor->DebugGetState().markdownPreparationAdvances;
        editor->SetMarkdownAnalysisProfile(profile);
        RenderEditor(window);
        Require(editor->GetMarkdownSyntax() == syntax && editor->DebugGetState().markdownPreparationAdvances == advances,
                "unchanged source/profile and geometry-only work retain completed pure analysis");
    }
    const auto held = editor->GetMarkdownSyntax();
    editor->SetText(std::wstring(32000u, L'&'));
    Require(window.Host().RenderInitialFrameForShow(), "native parser owner turn starts");
    const auto pending = editor->DebugGetState();
    Require(pending.markdownPreparationPending && pending.lastAdvanceMarkdownWork <= 4096u && ! editor->GetMarkdownSyntax(),
            "native parser yields with no stale current-source result");
    TextInputState input;
    Require(editor->ExportTextInputState(input) && input.text == editor->GetText(), "pending analysis preserves exact source adapter state without OS focus");
    Require(editor->OnChar(window.Host(), L'\u03b2', 0u) && editor->Undo(), "pending analysis does not block Unicode source transactions");
    Require(editor->DebugGetState().markdownPreparationAdvances == pending.markdownPreparationAdvances && held->Source() == source,
            "source input and reads never drain or mutate retained syntax");
    Require(window.Host().RenderInitialFrameForShow() && editor->DebugGetState().markdownPreparationPending, "resume only latest native parser request");
    editor->SetVisible(false);
    Require(! editor->DebugGetState().markdownPreparationPending, "hidden native control cancels staged parsing");
    editor->SetVisible(true);
    Require(window.Host().RenderInitialFrameForShow() && editor->DebugGetState().markdownPreparationPending, "shown native control resumes staged parsing");
    window.Host().DebugSimulateDeviceLoss();
    Require(! editor->DebugGetState().markdownPreparationPending, "native resource loss retires the pending owner job");
    editor->SetText(source);
    RenderEditor(window);
    const auto fresh = editor->GetMarkdownSyntax();
    Require(fresh && fresh->Revision() == editor->Revision() && fresh->Source() == source && fresh != held, "only final revision can become current");
    editor->SetSelection({2u, 2u});
    Require(editor->ReplaceSelection(L"x") && editor->Undo(), "analysis-enabled edit undo retains source");
    const auto selection = editor->GetSelection();
    const auto revision  = editor->Revision();
    RejectEditor([&] { editor->SetMarkdownAnalysisProfile(static_cast<MarkdownProfile>(99)); });
    Require(editor->GetSelection() == selection && editor->Revision() == revision && editor->CanRedo(), "invalid profile is atomic");
    editor->SetMarkdownAnalysisProfile(std::nullopt);
    RenderEditor(window);
    Require(! editor->GetMarkdownSyntax() && editor->CanRedo() && fresh->Source() == source, "opt-out releases owner analysis while held revisions survive");
}
} // namespace

namespace
{
void TestRichTextEditorRemoteImagePolicy()
{
    AttachedHostWindow window;
    auto owned   = std::make_unique<RichTextEditor>();
    auto* editor = owned.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    editor->SetDocumentFilePath(L"D:\\Writing\\Ideas.md");
    window.Host().SetRoot(std::move(owned));
    const std::wstring source = L"![local](cover.bmp)\n\n![remote](HTTPS://EXAMPLE.INVALID/Exact.bmp?q=Value)\n";
    editor->SetText(source);
    RenderEditor(window);
    std::vector<RichImageRequest> requests(0u);
    editor->TakePendingImageRequests(requests);
    Require(! editor->RemoteImagesEnabled() && requests.size() == 1u && requests[0].SourceKind() == RichImageSourceKind::LocalFile &&
                requests[0].FilePath() == L"D:\\Writing\\cover.bmp" && requests[0].Source() == requests[0].FilePath(),
            "remote image requests are absent by default while local requests retain their explicit file base");
    const auto local     = requests[0];
    const auto revision  = editor->Snapshot();
    const auto selection = editor->GetSelection();
    constexpr std::array<std::byte, 4> red{std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255}};
    constexpr std::array<std::byte, 4> blue{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
    std::shared_ptr<const RichImage> localPixels, remotePixels;
    RequireSucceeded(RichImage::CreatePbgra(1, 1, red, localPixels), "local privacy-policy image fixture");
    RequireSucceeded(RichImage::CreatePbgra(1, 1, blue, remotePixels), "remote privacy-policy image fixture");
    editor->SetRemoteImagesEnabled(true);
    Require(! local.Cancelled() && editor->CompleteImageRequest(local, localPixels),
            "opt-in preserves a dispatched local job and accepts its completion before policy preparation");
    RenderEditor(window);
    editor->TakePendingImageRequests(requests);
    Require(editor->RemoteImagesEnabled() && requests.size() == 1u && requests[0].SourceKind() == RichImageSourceKind::RemoteHttp &&
                requests[0].Source() == L"https://example.invalid/Exact.bmp?q=Value" && requests[0].FilePath().empty(),
            "explicit opt-in emits an immutable HTTP source with exact path/query and no misleading local path");
    const auto remote = requests[0];
    Require(editor->CompleteImageRequest(remote, remotePixels), "current opted-in remote pixels publish only on their owner");
    RenderEditor(window);
    Require(editor->GetImageResourceStatistics().readySources == 2u && editor->GetImageResourceStatistics().decodedBytes == 8u,
            "local and remote resources share the canonical decoded memory budget");
    editor->SetRemoteImagesEnabled(false);
    Require(! editor->RemoteImagesEnabled() && editor->GetImageResourceStatistics().readySources == 1u &&
                editor->GetImageResourceStatistics().decodedBytes == 4u && ! editor->CompleteImageRequest(remote, remotePixels),
            "opt-out immediately evicts remote pixels and rejects old completion while retaining local pixels");
    RenderEditor(window);
    editor->TakePendingImageRequests(requests);
    Require(requests.empty(), "off-by-default/disabled remote sources do not form a retry loop");
    editor->SetRemoteImagesEnabled(true);
    RenderEditor(window);
    editor->TakePendingImageRequests(requests);
    Require(requests.size() == 1u && requests[0] != remote && ! requests[0].Cancelled(), "a later opt-in owns a new remote request identity");
    const auto pending = requests[0];
    editor->SetRemoteImagesEnabled(false);
    Require(pending.Cancelled() && ! editor->CompleteImageRequest(pending, remotePixels),
            "disabling cancels an in-flight remote request before any later owner preparation");
    RenderEditor(window);
    Require(editor->GetText() == source && editor->Snapshot().IsSameRevision(revision) && editor->GetSelection() == selection && ! editor->CanUndo(),
            "all privacy transitions retain exact source, revision, selection and history");
    using DxUi::RichText::Detail::ResolveRemoteImageSource;
    for (const auto* invalid : {L"http://",
                                L"//example.invalid/a.png",
                                L"http://user:secret@example.invalid/a",
                                L"http://example.invalid/a#fragment",
                                L"http://example.invalid/a%",
                                L"http://example.invalid:0/a",
                                L"http://example.invalid:65536/a",
                                L"http://example.invalid\\a",
                                L"data:image/png,a",
                                L"http://example.invalid/a\r\nCookie: secret"})
        Require(! ResolveRemoteImageSource(invalid), "credentialed, ambiguous and unsupported sources are never remote image requests");
}
} // namespace

void RunRichTextEditorTests()
{
    ScopedNonActivatingTestWindows nonActivating;
    DXUI_RUN_TEST(TestRichTextEditorTransactionsAndValidation);
    DXUI_RUN_TEST(TestRichTextEditorUnicodeCharacterTransactions);
    DXUI_RUN_TEST(TestRichTextEditorFormattingAndModePreserveSource);
    DXUI_RUN_TEST(TestRichTextEditorCallbackRetirementAndFailure);
    DXUI_RUN_TEST(TestRichTextEditorSelectionPublicationAndRetirement);
    DXUI_RUN_TEST(TestRichTextEditorCommittedTextThenSelectionCallbacks);
    DXUI_RUN_TEST(TestRichTextEditorSelectionGestureAndTextServicePublication);
    DXUI_RUN_TEST(TestRichTextEditorNativeHostAndLayoutReuse);
    DXUI_RUN_TEST(TestRichTextEditorPreparedSourceInkPreservesGeometryAndSelection);
    DXUI_RUN_TEST(TestRichTextEditorDocumentFocusChromeDoesNotRetireTextGeometry);
    DXUI_RUN_TEST(TestRichTextEditorNativeUnicodeRoutingAndNavigation);
    DXUI_RUN_TEST(TestRichTextEditorClipboardAtomicityAndSourceContract);
    DXUI_RUN_TEST(TestRichTextEditorScrollAndHostClock);
    DXUI_RUN_TEST(TestRichTextEditorCatalogAndRebinding);
    // Repeated timing is an explicit diagnostic. Ordinary native behavior is
    // independently covered below; Debug/ASan need not repeat five timing trials.
    if (GetDxUiTestFilter().active)
        DXUI_RUN_TEST(TestRichTextEditorRetainedPreparationFixedTrials);
    DXUI_RUN_TEST(TestRichTextEditorNativeChunkReuseAndRetirement);
    DXUI_RUN_TEST(TestRichTextEditorNativeCompatibilityWorkIsReported);
    DXUI_RUN_TEST(TestRichTextEditorNativeCooperativePreparation);
    DXUI_RUN_TEST(TestRichTextEditorNativeMessageSchedulingAndFailure);
    DXUI_RUN_TEST(TestRichTextEditorNativeDenseSourceAndSplitScheduling);
    DXUI_RUN_TEST(TestRichTextEditorNativeHostVisibilityAndZeroHeight);
    DXUI_RUN_TEST(TestRichTextEditorNativePreparationCanReplaceRoot);
    DXUI_RUN_TEST(TestRichTextEditorNativeLosslessMarkdownPreparation);
    DXUI_RUN_TEST(TestRichTextEditorNativeBlockGrammarAndIdentity);
    DXUI_RUN_TEST(TestRichTextEditorNativeInlineGrammarAndReferenceEdits);
    DXUI_RUN_TEST(TestRichTextEditorNativeFrontMatterFencesAndSourceEdits);
    DXUI_RUN_TEST(TestRichTextEditorNativeFlowchartRenderingAndFallbacks);
    DXUI_RUN_TEST(TestRichTextEditorNativeDiagramViewCapacityRecovery);
    DXUI_RUN_TEST(TestRichTextEditorNativeDiagramSelectionRaster);
    DXUI_RUN_TEST(TestRichTextEditorNativeFootnoteBlockSourceEdits);
    DXUI_RUN_TEST(TestRichTextEditorNativeFootnoteResolutionAndSourceEdits);
    DXUI_RUN_TEST(TestRichTextEditorNativeParserCapacitySourceFallback);
    DXUI_RUN_TEST(TestRichTextEditorNativeRetainedObjectSourceEditing);
    DXUI_RUN_TEST(TestRichTextEditorNativeImagePathPaste);
    DXUI_RUN_TEST(TestRichTextEditorNativeFileReferencePaste);
    DXUI_RUN_TEST(TestRichTextNativeImageDecoder);
    DXUI_RUN_TEST(TestRichTextEditorNativeImageResources);
    DXUI_RUN_TEST(TestRichTextEditorRemoteImagePolicy);
    DXUI_RUN_TEST(TestRichTextEditorNativeImagePixels);
}
