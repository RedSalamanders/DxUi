#include "Controls.Tests.TextInputSyncOnce.h"

#include "Controls.Tests.DxUiTestHelpers.h"

#include <cmath>
#include <cwctype>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace DxUi;

namespace
{
// A WARP host shown without activation (the window sits off every monitor), so the native text-input session exists while
// the desktop's focus stays where it is.
struct SyncOnceWindow final
{
    AttachedHostWindow window{WindowHost::PresentationMode::HwndSwapChain, true};
    SyncOnceWindow()
    {
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    }
    SyncOnceWindow(const SyncOnceWindow&)            = delete;
    SyncOnceWindow& operator=(const SyncOnceWindow&) = delete;
    SyncOnceWindow(SyncOnceWindow&&)                 = delete;
    SyncOnceWindow& operator=(SyncOnceWindow&&)      = delete;
    [[nodiscard]] WindowHost& Host() noexcept
    {
        return window.Host();
    }
    void Prepare()
    {
        WindowHostBitmapCapture capture;
        Require(Host().DebugCaptureBitmap(capture) || Host().DebugCaptureBitmap(capture, true), "sync-once fixture: a frame is prepared");
    }
    void Char(wchar_t character)
    {
        static_cast<void>(SendMessageW(window.Hwnd(), WM_CHAR, character, 1));
    }
    void Key(UINT key)
    {
        static_cast<void>(SendMessageW(window.Hwnd(), WM_KEYDOWN, key, 1));
        static_cast<void>(SendMessageW(window.Hwnd(), WM_KEYUP, key, static_cast<LPARAM>(0xC0000001u)));
    }
    [[nodiscard]] uint64_t Synchronizations() const
    {
        return const_cast<SyncOnceWindow*>(this)->Host().DebugGetNativeTextInputEventCounters().synchronizationCount;
    }
    // The host's caret, as last computed and placed, equals the control's caret at the cached caret index now.
    void RequireCaretCurrent(const Control& control, const char* context)
    {
        NativeTextInputState state;
        Require(Host().DebugGetNativeTextInputState(state), context);
        const std::optional<D2D1_RECT_F> expected = control.TryGetTextInputCaretRect(Host(), state.caretIndex);
        D2D1_RECT_F actual{};
        RECT screen{};
        const bool hostHasCaret = Host().DebugGetNativeTextInputCaretRect(actual, screen);
        const auto equalDip     = [](float left, float right) { return std::fabs(left - right) < 0.01f; };
        Require(expected.has_value() == hostHasCaret, context);
        if (expected)
            Require(equalDip(expected->left, actual.left) && equalDip(expected->top, actual.top) && equalDip(expected->right, actual.right) &&
                        equalDip(expected->bottom, actual.bottom),
                    context);
    }
};
} // namespace

void TestTextInputSyncOnceRefreshesTheCaretAfterAFormatterScrolls()
{
    // A single-line field with overflowing text whose change callback normalizes it: the callback's SetText resets the
    // scroll and synchronizes, then the field scrolls its caret back into view without synchronizing again.
    SyncOnceWindow fixture;
    auto root         = std::make_unique<Panel>();
    auto* const field = root->AddChild<TextField>(std::wstring(60u, L'a'));
    field->SetBounds(D2D1::RectF(8.0f, 8.0f, 128.0f, 40.0f));
    bool formatting = false;
    field->SetOnTextChanged([&](std::wstring_view text)
    {
        if (formatting)
            return;
        formatting = true;
        std::wstring upper(text);
        for (wchar_t& unit : upper)
            unit = static_cast<wchar_t>(std::towupper(unit));
        field->SetText(std::move(upper));
        formatting = false;
    });
    fixture.Host().SetRoot(std::move(root));
    fixture.Host().SetFocusControl(field, false);
    field->SetSelectionRange(60u, 60u);
    fixture.Prepare();
    fixture.Char(L'a');
    Require(field->GetText() == std::wstring(61u, L'A'), "the formatter normalized the typed text");
    fixture.RequireCaretCurrent(*field, "after a formatter's SetText and the field's scroll, the host caret is at the scrolled caret");
}

void TestTextInputSyncOnceRefreshesTheCaretAfterARichCallbackScrolls()
{
    // The editor synchronizes the moved selection, then its selection callback scrolls, which does not synchronize.
    SyncOnceWindow fixture;
    std::wstring document;
    for (int line = 0; line < 200; ++line)
        document += L"line " + std::to_wstring(line) + L" of a scrolled document\r\n";
    auto owned         = std::make_unique<RichTextEditor>(document);
    auto* const editor = owned.get();
    editor->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 160.0f));
    fixture.Host().SetRoot(std::move(owned));
    fixture.Host().SetFocusControl(editor, false);
    const size_t caret = *editor->Snapshot().LineStart(3u);
    editor->SetSelection(RichText::Selection{caret, caret});
    fixture.Prepare();
    bool scroll = true;
    editor->SetOnSelectionChanged([&]
    {
        if (! std::exchange(scroll, false))
            return;
        const auto info = editor->GetScrollInfo();
        Require(info.has_value(), "the editor's scroll geometry is prepared");
        static_cast<void>(editor->SetScrollOffsetDip(info->offsetDip + 6.0f));
    });
    const auto before = editor->GetScrollInfo();
    fixture.Key(VK_RIGHT);
    Require(! scroll && editor->GetSelection().caret == caret + 1u, "the arrow key moved the caret and the callback ran");
    Require(before && editor->GetScrollInfo() && editor->GetScrollInfo()->offsetDip != before->offsetDip, "the callback scrolled the editor");
    fixture.RequireCaretCurrent(*editor, "after a selection callback scrolls the editor, the host caret follows the scroll");
}

void TestTextInputSyncOnceSynchronizesAPasswordRevealChangeMadeAfterTheControlsSync()
{
    // The callback synchronizes (SetText), then changes the reveal state, which does not synchronize: the host must.
    SyncOnceWindow fixture;
    auto root         = std::make_unique<Panel>();
    auto* const field = root->AddChild<TextField>(L"secret");
    field->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    field->SetMasked(true);
    field->SetPasswordRevealMode(PasswordRevealMode::Peek);
    bool inside = false;
    field->SetOnTextChanged([&](std::wstring_view text)
    {
        if (inside)
            return;
        inside = true;
        field->SetText(std::wstring(text));
        field->SetPasswordRevealState(PasswordRevealState::Visible);
        inside = false;
    });
    fixture.Host().SetRoot(std::move(root));
    fixture.Host().SetFocusControl(field, false);
    field->SetSelectionRange(6u, 6u);
    fixture.Prepare();
    fixture.Char(L'x');
    NativeTextInputState state;
    Require(fixture.Host().DebugGetNativeTextInputState(state), "the masked field has a native session");
    Require(field->GetPasswordRevealState() == PasswordRevealState::Visible && state.passwordRevealState == PasswordRevealState::Visible,
            "a reveal state changed after the control's own synchronization reaches the host cache");
}

void TestTextInputSyncOnceSkipsWhenTheControlAlreadySynchronizedTheSameState()
{
    // A character typed into a rich editor: the editor synchronizes what it shows during dispatch, so the host adds no
    // synchronization of its own; the same character delivered straight to the control counts the same.
    SyncOnceWindow fixture;
    auto owned         = std::make_unique<RichTextEditor>(L"alpha beta gamma");
    auto* const editor = owned.get();
    editor->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 120.0f));
    fixture.Host().SetRoot(std::move(owned));
    fixture.Host().SetFocusControl(editor, false);
    editor->SetSelection(RichText::Selection{5u, 5u});
    fixture.Prepare();
    const uint64_t beforeMessage = fixture.Synchronizations();
    fixture.Char(L'x');
    const uint64_t throughHost = fixture.Synchronizations() - beforeMessage;
    fixture.Prepare();
    const uint64_t beforeDirect = fixture.Synchronizations();
    Require(editor->OnChar(fixture.Host(), L'y', 0u), "the editor takes the character directly");
    const uint64_t direct = fixture.Synchronizations() - beforeDirect;
    Require(editor->GetText() == L"alphaxy beta gamma", "both characters were typed");
    Require(throughHost >= 1u && throughHost == direct, "the host adds no synchronization to the one the editor already made");
    fixture.RequireCaretCurrent(*editor, "a skipped synchronization still leaves the host caret current");
}
