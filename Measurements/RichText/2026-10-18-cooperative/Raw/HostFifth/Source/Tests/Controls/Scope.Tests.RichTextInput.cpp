#include "Scope.Tests.RichTextInput.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/Embedded.h>
#include <imm.h>

namespace
{
using namespace DxUi;
using namespace DxUi::RichText;

// Expose the protected adapter to exercise malformed service input independently
// of TSF's own validation. Production callers go through ControlHost/EmbeddedHost.
class ServiceRichEditor final : public RichTextEditor
{
public:
    using RichTextEditor::ExportTextInputState;
    using RichTextEditor::ImportTextInputState;
    using RichTextEditor::RichTextEditor;
};

void TestRichTextTransientSourcePreservesHistory()
{
    Document document(L"base");
    document.SetSelection({4, 4});
    Require(document.ReplaceSelection(L"!") && document.Undo(), "fixture retains redo before composition");
    const auto revision = document.Revision();
    Require(document.ReplaceTransient(L"base\u4eee", {5, 5}), "preview changes source");
    Require(! document.CanUndo() && document.CanRedo() && document.Revision() > revision, "preview preserves both histories with a fresh revision");
    Require(document.ReplaceTransient(L"base", {4, 4}) && document.CanRedo(), "cancel restores entry text without destroying redo");
    Require(document.Redo() && document.Text() == L"base!", "pre-composition redo still works");
    const auto before = document.Snapshot();
    try
    {
        document.ReplaceTransient(L"\U0001f600", {1, 1});
        Require(false, "split scalar preview must fail");
    }
    catch (const std::out_of_range&)
    {
    }
    Require(document.Text() == before.Text(), "rejected preview leaves source unchanged");
}

void TestRichTextServiceValidationAndGeometry()
{
    AttachedHostWindow window;
    auto editor = std::make_unique<ServiceRichEditor>(L"# **\u03b2\U0001f600**\r\nnext");
    auto* raw   = editor.get();
    window.Host().SetRoot(std::move(editor));
    Require(PrepareNativeContentForTests(window.Host()), "prepare native rich service geometry");
    window.Host().SetFocusControl(raw, false);
    TextInputState state;
    Require(raw->ExportTextInputState(state) && state.multiline && ! state.masked, "service exposes Unicode multiline source");
    const auto original = state;
    state.caretIndex    = 6; // Between the emoji's surrogate halves.
    Require(! raw->ImportTextInputState(window.Host(), state, true), "reject split supplementary ACP");
    state            = original;
    state.text       = std::wstring(1, 0xd800);
    state.caretIndex = 0;
    Require(! raw->ImportTextInputState(window.Host(), state, false), "reject malformed preview UTF-16");
    state.text = std::wstring(Document::MaximumUnits + 1, L'x');
    Require(! raw->ImportTextInputState(window.Host(), state, true), "reject oversized service transaction");
    Require(raw->GetText() == original.text && ! raw->CanUndo(), "invalid service changes are atomic");
    const auto hidden = raw->TryGetTextInputRangeRects(window.Host(), 0, 4);
    const auto glyph  = raw->TryGetTextInputRangeRects(window.Host(), 4, 5);
    Require(hidden && hidden->empty() && glyph && ! glyph->empty(), "hidden heading/bold markers have no pixels but source glyph has geometry");
    const auto caret = raw->TryGetTextInputCaretRect(window.Host(), 4);
    Require(caret.has_value(), "source caret maps through heading and inline projection");
    const auto hit = raw->TryHitTestTextInputPoint(window.Host(), {caret->left, (caret->top + caret->bottom) / 2});
    Require(hit && *hit == 4, "projected point maps back after hidden opening markers");
    raw->SetViewMode(ViewMode::Source);
    Require(PrepareNativeContentForTests(window.Host()), "prepare visible source markers");
    const auto source = raw->TryGetTextInputRangeRects(window.Host(), 0, 4);
    Require(source && ! source->empty(), "source mode exposes marker geometry");
    raw->SetReadOnly(true);
    state      = original;
    state.text = L"forbidden";
    Require(! raw->ImportTextInputState(window.Host(), state, true) && raw->GetText() == original.text, "read-only service writes are rejected");
    raw->SetVisible(false);
    Require(! raw->TryGetTextInputCaretRect(window.Host(), 0), "hidden editor retires service geometry");
}

void ImeMessage(ControlHost& host, UINT message, LPARAM flags = 0)
{
    bool handled = false;
    host.HandleMessage(nullptr, message, 0, flags, handled);
    Require(handled, "rootless native IMM message routes to production host adapter");
}

// A rootless ControlHost exercises the production IMM protocol without an HWND
// or keyboard focus. It does not qualify a real IME or its candidate window.
void TestRichTextNativeImeCommitCancelAndRetirement()
{
    ControlHost host;
    auto editor = std::make_unique<RichTextEditor>(L"**a**");
    auto* raw   = editor.get();
    host.SetRoot(std::move(editor));
    host.SetFocusControl(raw, false);
    raw->SetSelection({3, 3});
    size_t notices = 0;
    raw->SetOnTextChanged([&] { ++notices; });
    ImeMessage(host, WM_IME_STARTCOMPOSITION);
    NativeTextInputImePayload payload;
    payload.hasCompositionString = true;
    payload.compositionString    = L"\u4eee";
    host.DebugSetNativeTextInputImePayloadForTest(payload);
    ImeMessage(host, WM_IME_COMPOSITION, GCS_COMPSTR);
    Require(raw->GetText() == L"**a\u4eee**" && notices == 0 && ! raw->CanUndo(), "native preview is silent and creates no undo entry");
    ImeMessage(host, WM_IME_ENDCOMPOSITION);
    Require(raw->GetText() == L"**a**" && raw->GetSelection() == Selection{3, 3} && notices == 0, "native cancel restores exact source and selection");
    ImeMessage(host, WM_IME_STARTCOMPOSITION);
    host.DebugSetNativeTextInputImePayloadForTest(payload);
    ImeMessage(host, WM_IME_COMPOSITION, GCS_COMPSTR);
    payload                 = {};
    payload.hasResultString = true;
    payload.resultString    = L"\u4eee";
    host.DebugSetNativeTextInputImePayloadForTest(payload);
    ImeMessage(host, WM_IME_COMPOSITION, GCS_RESULTSTR);
    ImeMessage(host, WM_IME_ENDCOMPOSITION);
    Require(notices == 1 && raw->Undo() && raw->GetText() == L"**a**" && ! raw->CanUndo(), "native result equal to preview is one notifying undo transaction");
    raw->SetOnTextChanged([&] { host.SetRoot(std::make_unique<Label>(L"replacement")); });
    ImeMessage(host, WM_IME_STARTCOMPOSITION);
    host.DebugSetNativeTextInputImePayloadForTest(payload);
    ImeMessage(host, WM_IME_COMPOSITION, GCS_RESULTSTR);
    Require(dynamic_cast<Label*>(host.GetRoot()) && ! host.HasActiveNativeTextInputSession(), "native commit delegate can retire editor and text session");
}

void TestRichTextEmbeddedCompositionRevisionsAndRebinding()
{
    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "embedded rich input WARP fixture");
    std::shared_ptr<GraphicsDevice> graphics;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "shared rich input graphics");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(graphics), "attach embedded rich input view");
    auto root = std::make_unique<Panel>();
    auto* raw = root->AddChild<RichTextEditor>(L"**a**");
    raw->SetBounds(D2D1::RectF(0, 0, 400, 180));
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240), "prepare embedded rich input");
    view.Controls().SetFocusControl(raw, false);
    raw->SetSelection({3, 3});
    RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240), "prepare selected embedded input");
    size_t notices = 0;
    raw->SetOnTextChanged([&] { ++notices; });
    EmbeddedTextInputSnapshot before;
    Require(view.ReadTextInput(before) == S_OK && before.caretBoundsDip, "embedded source snapshot and prepared caret");
    auto preview                       = before.state;
    preview.text                       = L"**a\u4eee**";
    preview.caretIndex                 = 4;
    preview.compositionStartIndex      = 3;
    preview.compositionEndIndex        = 4;
    preview.conversionTargetStartIndex = 3;
    preview.conversionTargetEndIndex   = 4;
    Require(view.ApplyTextInput(before.revision, preview, EmbeddedTextInputAction::Preview) == S_OK, "embedded preview accepted");
    Require(raw->GetText() == preview.text && notices == 0 && ! raw->CanUndo(), "embedded preview preserves history and notification boundary");
    Require(view.ApplyTextInput(before.revision, preview, EmbeddedTextInputAction::Commit) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH),
            "stale service revision rejected");
    RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240), "prepare composition underline and geometry");
    RequireSucceeded(gpu.Resize(480, 240), "composition capture target");
    gpu.Bind();
    RequireSucceeded(view.Composite(gpu.context.get(), gpu.Viewport()), "composite prepared composition pixels");
    RequireSucceeded(gpu.Save(".build/test-artifacts/rich-d2d-composition.png"), "capture composition through the non-activating WARP harness");
    EmbeddedTextInputSnapshot live;
    Require(view.ReadTextInput(live) == S_OK && live.state.compositionStartIndex == 3, "composition range is published");
    Require(view.ApplyTextInput(live.revision, live.state, EmbeddedTextInputAction::Cancel) == S_OK, "embedded cancel accepted");
    Require(raw->GetText() == before.state.text && raw->GetSelection() == Selection{3, 3} && notices == 0,
            "embedded cancellation restores source and selection silently");
    Require(view.ReadTextInput(live) == S_OK, "read after cancellation");
    Require(view.ApplyTextInput(live.revision, preview, EmbeddedTextInputAction::Preview) == S_OK, "second preview");
    Require(view.ReadTextInput(live) == S_OK, "read final preview revision");
    auto result = preview;
    result.compositionStartIndex.reset();
    result.compositionEndIndex.reset();
    result.conversionTargetStartIndex.reset();
    result.conversionTargetEndIndex.reset();
    Require(view.ApplyTextInput(live.revision, result, EmbeddedTextInputAction::Commit) == S_OK && notices == 1, "embedded final preview commits exactly once");
    Require(raw->Undo() && raw->GetText() == before.state.text && ! raw->CanUndo(), "embedded composition is one undo entry");
    RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240, 144), "DPI reprepare retains input identity");
    Require(view.ReadTextInput(live) == S_OK && live.focusId == before.focusId, "DPI preserves focused service identity");
    raw->SetOnTextChanged([&] { view.Controls().SetRoot(std::make_unique<Label>(L"retired")); });
    auto committed       = live.state;
    committed.text       = L"replacement";
    committed.caretIndex = committed.text.size();
    committed.selectionAnchorIndex.reset();
    Require(view.ApplyTextInput(live.revision, committed, EmbeddedTextInputAction::Commit) == S_OK, "embedded commit tolerates destructive delegate");
    Require(view.ReadTextInput(live) == S_FALSE, "retired embedded editor has no readable text session");
}

void TestRichTextEmbeddedMappedGeometry()
{
    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "mapped rich input WARP fixture");
    std::shared_ptr<GraphicsDevice> graphics;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "mapped rich input graphics");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(graphics), "attach mapped rich input");
    auto root    = std::make_unique<RichTextEditor>(L"**alpha**\r\n");
    auto* editor = root.get();
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240), "prepare mapped rich input");
    view.Controls().SetFocusControl(editor, false);
    std::wstring source = L"**alpha**\r\n";
    for (int line = 0; line < 80; ++line)
        source += L"wrapped Unicode \u03b2 \U0001f600 text across the viewport\r\n";
    editor->SetText(source);
    EmbeddedTextInputSnapshot snapshot;
    Require(view.ReadTextInput(snapshot) == S_OK && ! snapshot.caretBoundsDip, "dirty source publishes state while withholding stale geometry");
    D2D1_RECT_F bounds{};
    bool clipped = false;
    Require(view.GetTextInputRangeBounds(snapshot.revision, 0, source.size(), bounds, clipped) == S_FALSE, "geometry waits for coherent preparation");
    for (const float dpi : {96.0f, 144.0f})
    {
        RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240, dpi), "prepare mapped geometry at DPI");
        Require(view.ReadTextInput(snapshot) == S_OK && snapshot.caretBoundsDip && snapshot.viewportBoundsDip, "prepared source geometry is available");
        Require(view.GetTextInputRangeBounds(snapshot.revision, 0, source.size(), bounds, clipped) == S_OK && clipped,
                "offscreen lines set the service clipping flag");
        Require(bounds.top >= snapshot.viewportBoundsDip->top && bounds.bottom <= snapshot.viewportBoundsDip->bottom,
                "service output clips to the prepared viewport");
        Require(view.GetTextInputRangeBounds(snapshot.revision, 0, 2, bounds, clipped) == S_OK && clipped && bounds.right == bounds.left,
                "hidden marker service range is pixel-empty");
        const auto caret = editor->TryGetTextInputCaretRect(view.Controls(), 2);
        Require(caret.has_value(), "mapped source caret exists");
        size_t index = 0;
        Require(view.HitTestTextInput(snapshot.revision, {caret->left, (caret->top + caret->bottom) / 2}, index) == S_OK && index == 2,
                "service point hit maps after hidden opening markers");
        const auto visible = editor->GetVisibleSourceRange(view.Controls());
        Require(visible && visible->second < source.size(), "visible source range excludes unshown wrapped lines");
    }
}

void TestRichTextEmbeddedPreviewRebinding()
{
    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "rebind rich input WARP fixture");
    std::shared_ptr<GraphicsDevice> graphics;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "rebind shared graphics");
    EmbeddedHost first, second;
    RequireSucceeded(first.Attach(graphics), "attach first rich input host");
    RequireSucceeded(second.Attach(graphics), "attach second rich input host");
    auto root    = std::make_unique<Panel>();
    auto* panel  = root.get();
    auto* editor = panel->AddChild<RichTextEditor>(L"kept");
    editor->SetBounds(D2D1::RectF(0, 0, 400, 180));
    first.Controls().SetRoot(std::move(root));
    RequireSucceeded(PrepareEmbeddedContentForTests(first, 480, 240), "prepare first rich input host");
    first.Controls().SetFocusControl(editor, false);
    editor->SetSelection({4, 4});
    Require(editor->ReplaceSelection(L"!"), "prior independent rich edit");
    EmbeddedTextInputSnapshot before;
    Require(first.ReadTextInput(before) == S_OK, "read first rich input identity");
    auto preview                  = before.state;
    preview.text                  = L"kept!\u4eee";
    preview.caretIndex            = 6;
    preview.compositionStartIndex = 5;
    preview.compositionEndIndex   = 6;
    Require(first.ApplyTextInput(before.revision, preview, EmbeddedTextInputAction::Preview) == S_OK, "preview before rebinding");
    auto retained = panel->TakeChild(0);
    Require(retained.get() == editor && editor->GetText() == L"kept!", "detach cancels owned preview before changing host binding");
    second.Controls().SetRoot(std::move(retained));
    RequireSucceeded(PrepareEmbeddedContentForTests(second, 480, 240, 144), "prepare rebound rich input editor");
    second.Controls().SetFocusControl(editor, false);
    Require(first.ApplyTextInput(before.revision, preview, EmbeddedTextInputAction::Commit) != S_OK, "old view cannot commit through a rebound editor");
    Require(editor->Undo() && editor->GetText() == L"kept", "rebinding preserves pre-composition undo history");
    EmbeddedTextInputSnapshot rebound;
    Require(second.ReadTextInput(rebound) == S_OK && ! rebound.state.compositionStartIndex, "new host publishes restored source with no inherited composition");
    editor->SetReadOnly(true);
    Require(second.ReadTextInput(rebound) == S_OK && rebound.state.readOnly, "read-only policy synchronizes immediately");
    preview                       = rebound.state;
    preview.text                  = L"forbidden";
    preview.caretIndex            = preview.text.size();
    preview.compositionStartIndex = 0;
    preview.compositionEndIndex   = preview.text.size();
    Require(second.ApplyTextInput(rebound.revision, preview, EmbeddedTextInputAction::Preview) == E_ACCESSDENIED && editor->GetText() == L"kept",
            "read-only composition cannot change source");
}
} // namespace

void RunRichTextInputTests()
{
    ScopedNonActivatingTestWindows nonActivating;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Require(SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE, "rich input fixtures initialize or borrow a COM apartment for WIC");
    const auto apartment = wil::scope_exit([initialized]
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    DXUI_RUN_TEST(TestRichTextTransientSourcePreservesHistory);
    DXUI_RUN_TEST(TestRichTextServiceValidationAndGeometry);
    DXUI_RUN_TEST(TestRichTextNativeImeCommitCancelAndRetirement);
    DXUI_RUN_TEST(TestRichTextEmbeddedCompositionRevisionsAndRebinding);
    DXUI_RUN_TEST(TestRichTextEmbeddedMappedGeometry);
    DXUI_RUN_TEST(TestRichTextEmbeddedPreviewRebinding);
}
