#pragma once

#include "../Support/Support.Tests.InteractiveLease.h"

#include <CommCtrl.h>

#include <atomic>
#include <cstdint>
#include <format>
#include <string>

#pragma comment(lib, "comctl32.lib")
#pragma comment(                                                                                                                                               \
    linker,                                                                                                                                                    \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace DxUi::InteractiveLease
{
inline constexpr int kStartButton = 1001;

// A person-free answer, for the self-test: `button` is clicked once the dialog has been open for `afterMilliseconds`.
struct DialogScript
{
    int button                      = 0;
    std::uint64_t afterMilliseconds = 0u;
    bool silent                     = false; // No icon: a warning icon can play the system's warning sound, which a test run must not.
};

struct DialogState
{
    explicit DialogState(unsigned confirmSeconds) noexcept : countdown(confirmSeconds)
    {
    }

    TestSupport::ConfirmationCountdown countdown;
    const std::atomic<bool>* interrupted = nullptr;
    DialogScript script;
    unsigned shownSeconds            = 0xFFFFFFFFu; // The countdown the footer shows now.
    TestSupport::Confirmation answer = TestSupport::Confirmation::None;
    bool scriptClicked               = false;
};

[[nodiscard]] inline std::wstring CountdownText(const DialogState& state, std::uint64_t elapsedMilliseconds)
{
    if (state.countdown.Unlimited())
        return L"This waits until you choose.";
    return std::format(L"If nobody chooses, this cancels itself in {} s.", state.countdown.SecondsLeft(elapsedMilliseconds));
}

inline void ShowCountdown(HWND dialog, DialogState& state, std::uint64_t elapsedMilliseconds) noexcept
{
    const unsigned seconds = state.countdown.SecondsLeft(elapsedMilliseconds);
    if (seconds == state.shownSeconds)
        return;
    state.shownSeconds = seconds;
    try
    {
        const std::wstring text = CountdownText(state, elapsedMilliseconds);
        SendMessageW(dialog, TDM_SET_ELEMENT_TEXT, TDE_FOOTER, reinterpret_cast<LPARAM>(text.c_str()));
    }
    catch (const std::bad_alloc&)
    {
        // The footer keeps its last text; the countdown itself does not depend on it.
    }
}

// The confirmation's callback runs on the dialog's thread, between Windows messages. It ends the dialog itself when the time runs
// out or the run is stopped, so nothing waits on the dialog from outside, and nothing polls: the dialog's own timer calls it.
inline HRESULT CALLBACK DialogCallback(HWND dialog, UINT notification, WPARAM wParam, LPARAM, LONG_PTR data) noexcept
{
    auto* const state = reinterpret_cast<DialogState*>(data);
    switch (notification)
    {
        case TDN_CREATED:
            // The dialog is where the person looks, so it stays above the other windows, and it asks for the foreground.
            SetWindowPos(dialog, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetForegroundWindow(dialog);
            ShowCountdown(dialog, *state, 0u);
            break;
        case TDN_TIMER:
        {
            const std::uint64_t elapsed = wParam;
            if (state->interrupted != nullptr && state->interrupted->load())
            {
                state->answer = TestSupport::Confirmation::Interrupted;
                SendMessageW(dialog, TDM_CLICK_BUTTON, IDCANCEL, 0);
            }
            else if (state->countdown.Expired(elapsed))
            {
                state->answer = TestSupport::Confirmation::TimedOut;
                SendMessageW(dialog, TDM_CLICK_BUTTON, IDCANCEL, 0);
            }
            else if (state->script.button != 0 && ! state->scriptClicked && elapsed >= state->script.afterMilliseconds)
            {
                state->scriptClicked = true;
                SendMessageW(dialog, TDM_CLICK_BUTTON, static_cast<WPARAM>(state->script.button), 0);
            }
            else
            {
                ShowCountdown(dialog, *state, elapsed);
            }
            break;
        }
        case TDN_BUTTON_CLICKED:
            if (state->answer == TestSupport::Confirmation::None)
                state->answer = wParam == static_cast<WPARAM>(kStartButton) ? TestSupport::Confirmation::Started : TestSupport::Confirmation::Declined;
            break;
        default: break;
    }
    return S_OK;
}

// Asks the person to let the run take over the desktop. The default button is Cancel, so a key pressed by accident when the dialog
// appears (a person typing in another window) never starts a takeover; Start needs a click or its access key. When nobody answers
// within the request's time the answer is Cancel too. The dialog needs no desktop of its own choosing: it opens on the one this
// thread is on, which the self-test makes a private one.
[[nodiscard]] inline TestSupport::Confirmation Confirm(const TestSupport::LeaseRequest& request, const std::atomic<bool>& interrupted, DialogScript script = {})
{
    DialogState state(request.confirmSeconds);
    state.interrupted = &interrupted;
    state.script      = script;

    const std::wstring duration = TestSupport::FromUtf8(TestSupport::FormatApproximateDuration(request.estimateSeconds));
    const std::wstring content =
        std::format(L"The interactive tests ({}) need real keyboard focus, the foreground window and the pointer. For {} they activate their own "
                    L"windows, type, click and move the pointer, so keys and clicks of yours would go to them.\n\n"
                    L"Keep your hands off the keyboard and mouse until the warning at the top of the screen goes away. When the run ends, fails, "
                    L"times out or is stopped with Ctrl+C, your foreground window, its keyboard focus and the pointer position are put back.",
                    request.label,
                    duration);
    const std::wstring footer = CountdownText(state, 0u);

    const TASKDIALOG_BUTTON startButton{kStartButton, L"&Start the tests"};
    TASKDIALOGCONFIG config{};
    config.cbSize             = sizeof(config);
    config.dwFlags            = TDF_ALLOW_DIALOG_CANCELLATION | TDF_CALLBACK_TIMER | TDF_SIZE_TO_CONTENT;
    config.dwCommonButtons    = TDCBF_CANCEL_BUTTON;
    config.pszWindowTitle     = L"DxUi interactive tests";
    config.pszMainIcon        = script.silent ? nullptr : TD_WARNING_ICON;
    config.pszMainInstruction = L"Let the interactive tests take over this desktop?";
    config.pszContent         = content.c_str();
    config.pszFooter          = footer.c_str();
    config.cButtons           = 1u;
    config.pButtons           = &startButton;
    config.nDefaultButton     = IDCANCEL;
    config.pfCallback         = &DialogCallback;
    config.lpCallbackData     = reinterpret_cast<LONG_PTR>(&state);

    int pressed          = 0;
    const HRESULT result = TaskDialogIndirect(&config, &pressed, nullptr, nullptr);
    if (FAILED(result))
        return TestSupport::Confirmation::Unavailable;
    if (state.answer == TestSupport::Confirmation::None)
        state.answer = pressed == kStartButton ? TestSupport::Confirmation::Started : TestSupport::Confirmation::Declined;
    return state.answer;
}
} // namespace DxUi::InteractiveLease
