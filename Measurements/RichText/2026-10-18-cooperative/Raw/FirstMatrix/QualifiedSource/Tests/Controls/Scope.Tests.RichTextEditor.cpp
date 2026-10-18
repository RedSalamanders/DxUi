#include "Scope.Tests.RichTextEditor.h"
#include "../../src/Controls/TextClipboard.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/ControlCatalog.h>
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

void RunRichTextEditorTests()
{
    ScopedNonActivatingTestWindows nonActivating;
    DXUI_RUN_TEST(TestRichTextEditorTransactionsAndValidation);
    DXUI_RUN_TEST(TestRichTextEditorUnicodeCharacterTransactions);
    DXUI_RUN_TEST(TestRichTextEditorFormattingAndModePreserveSource);
    DXUI_RUN_TEST(TestRichTextEditorCallbackRetirementAndFailure);
    DXUI_RUN_TEST(TestRichTextEditorNativeHostAndLayoutReuse);
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
    DXUI_RUN_TEST(TestRichTextEditorNativePreparationCanReplaceRoot);
}
