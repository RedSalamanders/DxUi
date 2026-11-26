#include "Controls.Tests.CaretClock.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "GraphicsFixture.h"
#include "Support/WorkCounters.h"

#include <iostream>
#include <optional>

// Every clause of W3.2 that does not need the fixture runner. FX1's clauses are in Embedded.Tests.PerformanceFixtures.h
// (embedded, fixture clock) and Controls.Tests.FrameSchedule.cpp (native, real time). Wakes here are those of an
// application that sleeps until the schedule's next deadline, which is what W3.5 publishes: each wake is one deadline.
namespace
{
using DxUi::ControlHost;
using DxUi::Detail::WorkCounterScope;
using Settings = DxUi::ControlHost::CaretBlinkSettingsForTest;

constexpr uint64_t kOriginMs = 1'000'000u;

struct SleepRun
{
    size_t wakes         = 0u;
    size_t invalidations = 0u;
};

// An application that sleeps until the next deadline: it wakes only at a due deadline, never every frame.
SleepRun SleepUntilDeadlines(ControlHost& host, uint64_t& now, uint64_t untilMs)
{
    SleepRun run;
    for (;;)
    {
        const auto wake = host.DebugGetFrameScheduleWakeForTest();
        Require(wake.kind != 1u, "a caret never asks for every frame");
        if (wake.kind == 0u || wake.dueTickMs > untilMs)
            break;
        Require(wake.dueTickMs > now, "a caret deadline lies ahead of the clock");
        Require(host.DebugGetTickRequestCountForTest() == 1u, "a caret holds exactly one request");
        now = wake.dueTickMs;
        host.DebugSetAnimationClockForTest(now);
        const uint64_t before = host.DebugGetInvalidateCount();
        static_cast<void>(host.DebugAnimationTickForTest(now));
        ++run.wakes;
        run.invalidations += static_cast<size_t>(host.DebugGetInvalidateCount() - before);
    }
    now = untilMs;
    host.DebugSetAnimationClockForTest(now);
    return run;
}

// An unattached window host with motion on, the given settings through the seam and the clock at kOriginMs.
void PrepareHost(ControlHost& host, std::optional<Settings> settings)
{
    EnableMotionForTest(host);
    host.DebugSetCaretBlinkSettingsForTest(settings);
    host.DebugSetAnimationClockForTest(kOriginMs);
}

[[nodiscard]] bool WakeIsNone(const ControlHost& host)
{
    return host.DebugGetFrameScheduleWakeForTest().kind == 0u;
}

[[nodiscard]] bool WakeIsDeadlineAt(const ControlHost& host, uint64_t dueTickMs)
{
    const auto wake = host.DebugGetFrameScheduleWakeForTest();
    return wake.kind == 2u && wake.dueTickMs == dueTickMs;
}

// Runs every-frame requests (a focus transition) to rest, 16 ms per frame, leaving deadlines alone.
void SettleTransitions(ControlHost& host, uint64_t& now)
{
    for (int frame = 0; frame < 200 && host.DebugGetFrameScheduleWakeForTest().kind == 1u; ++frame)
    {
        now += 16u;
        host.DebugSetAnimationClockForTest(now);
        static_cast<void>(host.DebugAnimationTickForTest(now));
    }
    Require(host.DebugGetFrameScheduleWakeForTest().kind != 1u, "transitions settle");
}

// Gives every child of `root` without bounds a 200 x 32 DIP slot: a caret owner with no area draws no caret and does not blink.
void SizeChildren(DxUi::Panel& root)
{
    float top = 8.0f;
    for (size_t index = 0u; index < root.GetLogicalChildCount(); ++index)
    {
        DxUi::Control* const child = root.GetLogicalChild(index);
        const D2D1_RECT_F bounds   = child->GetBounds();
        if (! (bounds.right > bounds.left && bounds.bottom > bounds.top))
            child->SetBounds(D2D1::RectF(8.0f, top, 208.0f, top + 32.0f));
        top += 40.0f;
    }
}

[[nodiscard]] std::shared_ptr<DxUi::GraphicsDevice> MakeCaretPool(GraphicsFixture& gpu)
{
    RequireSucceeded(gpu.Create(), "create the caret clock WARP device");
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    RequireSucceeded(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "share the caret clock graphics");
    return graphics;
}
} // namespace

// The settings table: each case runs a focused editable TextField for 10 s and counts the wakes of an application that
// sleeps until each deadline. Every wake is one phase flip and invalidates exactly once.
void TestCaretClockSettingsCases()
{
    struct Case
    {
        const char* name;
        Settings settings;
        bool reducedMotion;
        size_t wakes; // in 10 s
        bool endsSteady;
    };
    constexpr Case kCases[] = {
        {"default 530 ms, no timeout", {530u, 0u}, false, 18u, false},
        {"default 530 ms, 5 s timeout (Windows defaults)", {530u, 5000u}, false, 10u, true},
        {"short 200 ms", {200u, 0u}, false, 50u, false},
        {"long 1200 ms", {1200u, 0u}, false, 8u, false},
        {"INFINITE: steady", {INFINITE, 5000u}, false, 0u, true},
        {"timeout of 10 blinks", {530u, 5300u}, false, 10u, true},
        {"timeout of 5 blinks (odd: the caret is shown at the last flip before it)", {400u, 2000u}, false, 4u, true},
        {"timeout disabled (INFINITE)", {530u, INFINITE}, false, 18u, false},
        {"reduced motion: steady", {530u, 5000u}, true, 0u, true},
    };
    for (const Case& test : kCases)
    {
        DxUi::WindowHost host;
        PrepareHost(host, test.settings);
        auto theme          = host.GetTheme();
        theme.reducedMotion = test.reducedMotion;
        host.SetTheme(theme);
        auto root   = std::make_unique<DxUi::Panel>();
        auto* field = root->AddChild<DxUi::TextField>(L"alpha");
        SizeChildren(*root);
        host.SetRoot(std::move(root));
        host.SetFocusControl(field, false);
        Require(field->DebugIsCaretVisible(), "focus shows the caret at once");
        const bool blinks = test.wakes != 0u;
        if (blinks)
            Require(WakeIsDeadlineAt(host, kOriginMs + test.settings.blinkTimeMs), "the first flip is one period after focus");
        else
            Require(WakeIsNone(host), "a steady caret schedules nothing");

        uint64_t now       = kOriginMs;
        const SleepRun run = SleepUntilDeadlines(host, now, kOriginMs + 10'000u);
        std::cout << "    caret " << test.name << ": " << run.wakes << " wakes, " << run.invalidations << " invalidations in 10 s\n";
        Require(run.wakes == test.wakes, "the caret wakes once per phase flip, as the settings ask");
        Require(run.invalidations == run.wakes, "each flip invalidates once and nothing else does");
        if (test.endsSteady)
        {
            Require(WakeIsNone(host) && field->DebugIsCaretVisible(), "a steady or timed-out caret is shown and schedules nothing");
            const SleepRun after = SleepUntilDeadlines(host, now, now + 10'000u);
            Require(after.wakes == 0u, "0 wakes after the caret timeout");
        }
        else
            Require(host.DebugGetFrameScheduleWakeForTest().kind == 2u, "a blinking caret keeps exactly one deadline");
    }
}

// One clock serves TextField, editable ComboBox and RichTextEditor alike, at the system period (no 500 ms of its own).
void TestCaretClockServesEveryCaretOwner()
{
    for (int owner = 0; owner < 3; ++owner)
    {
        DxUi::WindowHost host;
        PrepareHost(host, Settings{530u, 0u});
        auto root                  = std::make_unique<DxUi::Panel>();
        DxUi::Control* control     = nullptr;
        DxUi::RichTextEditor* rich = nullptr;
        if (owner == 0)
            control = root->AddChild<DxUi::TextField>(L"alpha");
        else if (owner == 1)
        {
            auto* combo = root->AddChild<DxUi::ComboBox>();
            combo->SetEditable(true);
            combo->SetText(L"alpha");
            control = combo;
        }
        else
            control = rich = root->AddChild<DxUi::RichTextEditor>(L"alpha");
        SizeChildren(*root);
        host.SetRoot(std::move(root));
        host.SetFocusControl(control, false);
        Require(WakeIsDeadlineAt(host, kOriginMs + 530u), "every caret owner's first flip is one system period after focus");
        uint64_t now       = kOriginMs;
        const SleepRun run = SleepUntilDeadlines(host, now, kOriginMs + 10'000u);
        Require(run.wakes == 18u && run.invalidations == 18u, "every caret owner blinks at the system period, one wake per flip");
        if (rich)
        {
            rich->SetReadOnly(true);
            Require(WakeIsNone(host), "a read-only rich editor's caret is steady and schedules nothing");
            rich->SetReadOnly(false);
            Require(WakeIsDeadlineAt(host, now + 530u), "an editable rich editor blinks again from now");
        }
        host.SetFocusControl(nullptr, false);
        Require(WakeIsNone(host) && host.DebugGetTickRequestCountForTest() == 0u, "blur leaves nothing armed");
    }
}

// A read-only field draws a steady caret: shown on every frame, nothing scheduled, whatever input arrives.
void TestCaretClockReadOnlyCaretIsSteady()
{
    DxUi::WindowHost host;
    PrepareHost(host, Settings{530u, 5000u});
    auto root      = std::make_unique<DxUi::Panel>();
    auto* readOnly = root->AddChild<DxUi::TextField>(L"Read-only value");
    readOnly->SetReadOnly(true);
    auto* multiline = root->AddChild<DxUi::TextField>(L"one\ntwo");
    multiline->SetMultiline(true);
    multiline->SetReadOnly(true);
    SizeChildren(*root);
    host.SetRoot(std::move(root));
    for (DxUi::TextField* field : {readOnly, multiline})
    {
        host.SetFocusControl(field, false);
        Require(field->HasFocus() && field->DebugIsCaretVisible() && WakeIsNone(host), "a focused read-only caret is shown and schedules nothing");
        uint64_t now = kOriginMs;
        for (int step = 0; step < 100; ++step)
        {
            now += 100u;
            host.DebugSetAnimationClockForTest(now);
            static_cast<void>(host.DebugAnimationTickForTest(now));
            Require(field->DebugIsCaretVisible() && WakeIsNone(host), "the read-only caret is present at every step, with nothing armed");
        }
        static_cast<void>(field->OnKeyDown(host, VK_LEFT, 0u));
        static_cast<void>(field->OnChar(host, L'x', 0u));
        Require(field->DebugIsCaretVisible() && WakeIsNone(host), "input to a read-only field keeps its caret steady");
    }
    host.SetFocusControl(readOnly, false);
    host.DebugSetAnimationClockForTest(kOriginMs + 20'000u);
    readOnly->SetReadOnly(false);
    Require(WakeIsDeadlineAt(host, kOriginMs + 20'530u), "making the focused field editable starts the blink");
    readOnly->SetReadOnly(true);
    Require(WakeIsNone(host) && readOnly->DebugIsCaretVisible(), "making it read-only again leaves a steady, shown caret");
}

// Input shows the caret at once and restarts the phase (the next flip is one period later), and after the caret timeout it
// restarts blinking, as Windows does.
void TestCaretClockInputRestartsPhaseAndTimeout()
{
    DxUi::WindowHost host;
    PrepareHost(host, Settings{530u, 5000u});
    auto root   = std::make_unique<DxUi::Panel>();
    auto* field = root->AddChild<DxUi::TextField>(L"alpha");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 32.0f));
    SizeChildren(*root);
    host.SetRoot(std::move(root));
    host.SetFocusControl(field, false);
    uint64_t now = kOriginMs;
    Require(SleepUntilDeadlines(host, now, kOriginMs + 8'000u).wakes == 10u && WakeIsNone(host), "the caret times out");

    // A keystroke after the timeout.
    Require(field->OnChar(host, L'x', 0u), "the field takes the character");
    Require(field->DebugIsCaretVisible() && WakeIsDeadlineAt(host, now + 530u), "a keystroke after the timeout restarts the blink");
    Require(SleepUntilDeadlines(host, now, now + 800u).wakes == 1u && ! field->DebugIsCaretVisible(), "the restarted caret blinks off");

    // A selection move mid-phase: shown now, the next flip one period later.
    Require(field->OnKeyDown(host, VK_LEFT, 0u), "the field takes the arrow key");
    Require(field->DebugIsCaretVisible() && WakeIsDeadlineAt(host, now + 530u), "a selection move shows the caret and restarts the phase");
    host.DebugSetAnimationClockForTest(now += 300u);
    // A click (the field already has focus).
    static_cast<void>(field->OnMouseDown(host, D2D1::Point2F(4.0f, 4.0f), false, 0u));
    static_cast<void>(field->OnMouseUp(host, D2D1::Point2F(4.0f, 4.0f), false, 0u));
    Require(field->DebugIsCaretVisible() && WakeIsDeadlineAt(host, now + 530u), "a click restarts the phase");
    // A text-input import (IME composition update and commit) restarts it too.
    host.DebugSetAnimationClockForTest(now += 100u);
    field->SetSelectionRange(1u, 1u);
    Require(WakeIsDeadlineAt(host, now + 530u), "a selection set by the application restarts the phase");
    // The timeout counts from the latest input.
    const uint64_t input = now;
    const SleepRun run   = SleepUntilDeadlines(host, now, input + 20'000u);
    Require(run.wakes == 10u && WakeIsNone(host) && field->DebugIsCaretVisible(), "the timeout counts from the latest input");
}

// WM_SETTINGCHANGE (window host) and RefreshUserSettings (embedded application) apply a changed blink time or
// timeout at once: the caret shows and restarts; an unchanged setting restarts nothing. Reduced motion follows SetTheme.
void TestCaretClockSettingChangeWhileFocused()
{
    DxUi::WindowHost host;
    PrepareHost(host, Settings{530u, 0u});
    auto root   = std::make_unique<DxUi::Panel>();
    auto* field = root->AddChild<DxUi::TextField>(L"alpha");
    SizeChildren(*root);
    host.SetRoot(std::move(root));
    host.SetFocusControl(field, false);
    uint64_t now = kOriginMs;
    Require(SleepUntilDeadlines(host, now, kOriginMs + 800u).wakes == 1u && ! field->DebugIsCaretVisible(), "the caret is in its hidden phase");

    host.DebugSetCaretBlinkSettingsForTest(Settings{200u, 0u});
    Require(WakeIsDeadlineAt(host, kOriginMs + 1060u), "the seam alone changes nothing until the settings are read again");
    const uint64_t invalidations = host.DebugGetInvalidateCount();
    bool handled                 = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SETTINGCHANGE, 0u, 0, handled));
    Require(field->DebugIsCaretVisible() && WakeIsDeadlineAt(host, now + 200u) && host.DebugGetInvalidateCount() > invalidations,
            "WM_SETTINGCHANGE shows the caret and restarts it at the new period");
    host.DebugSetAnimationClockForTest(now += 50u);
    static_cast<void>(host.HandleMessage(nullptr, WM_SETTINGCHANGE, 0u, 0, handled));
    Require(WakeIsDeadlineAt(host, now - 50u + 200u), "an unrelated WM_SETTINGCHANGE leaves the phase alone");

    host.DebugSetCaretBlinkSettingsForTest(Settings{INFINITE, 0u});
    host.RefreshUserSettings();
    Require(WakeIsNone(host) && field->DebugIsCaretVisible(), "a change to INFINITE makes the caret steady at once");
    host.DebugSetCaretBlinkSettingsForTest(Settings{530u, 5000u});
    host.RefreshUserSettings();
    Require(WakeIsDeadlineAt(host, now + 530u), "a change back to a period blinks again from now");

    auto theme          = host.GetTheme();
    theme.reducedMotion = true;
    host.SetTheme(theme);
    Require(WakeIsNone(host) && field->DebugIsCaretVisible(), "reduced motion makes a focused caret steady at once");
    host.DebugSetAnimationClockForTest(now += 1000u);
    theme.reducedMotion = false;
    host.SetTheme(theme);
    Require(WakeIsDeadlineAt(host, now + 530u), "lifting reduced motion blinks again from now");
    host.DebugSetCaretBlinkSettingsForTest(std::nullopt);
}

// One caret per host: focus moving between two caret owners in one frame leaves one request, for the new owner, and a host
// with no focus, or focus on a control without a caret, schedules nothing.
void TestCaretClockOneOwnerPerHost()
{
    DxUi::WindowHost host;
    PrepareHost(host, Settings{530u, 0u});
    auto root    = std::make_unique<DxUi::Panel>();
    auto* field  = root->AddChild<DxUi::TextField>(L"alpha");
    auto* combo  = root->AddChild<DxUi::ComboBox>();
    auto* button = root->AddChild<DxUi::Button>(L"Apply");
    combo->SetEditable(true);
    combo->SetText(L"beta");
    SizeChildren(*root);
    host.SetRoot(std::move(root));
    Require(WakeIsNone(host), "a host with no focus schedules nothing");

    host.SetFocusControl(field, false);
    host.DebugSetAnimationClockForTest(kOriginMs + 300u);
    host.SetFocusControl(combo, false);
    Require(host.DebugGetTickRequestCountForTest() == 1u && WakeIsDeadlineAt(host, kOriginMs + 830u),
            "focus moved between caret owners leaves one deadline, the new owner's, one period after the move");
    Require(field->DebugIsCaretVisible() && combo->DebugIsCaretVisible(), "the old owner's caret state is reset; the new one shows");
    uint64_t now = kOriginMs + 300u;
    Require(SleepUntilDeadlines(host, now, now + 1'000u).wakes == 1u && ! combo->DebugIsCaretVisible() && field->DebugIsCaretVisible(),
            "only the new owner blinks");

    host.SetFocusControl(button, false);
    SettleTransitions(host, now); // the button's focus transition
    Require(WakeIsNone(host) && host.DebugGetTickRequestCountForTest() == 0u && combo->DebugIsCaretVisible(),
            "focus on a control without a caret schedules nothing");
    combo->SetEditable(false);
    host.SetFocusControl(combo, false);
    SettleTransitions(host, now);
    Require(WakeIsNone(host), "a focused non-editable combo box has no caret");
    combo->SetEditable(true);
    Require(host.DebugGetFrameScheduleWakeForTest().kind == 2u, "making the focused combo editable starts its caret");
    combo->SetEditable(false);
    Require(WakeIsNone(host), "making it non-editable withdraws the caret");
}

// Embedded: a hidden view wakes nothing and on show the caret is shown at once and restarts its phase; an open popup of
// the same host keeps the focus and the blink; a caret owner hidden while focused holds nothing.
void TestCaretClockEmbeddedHiddenPopupAndHiddenOwner()
{
    GraphicsFixture gpu;
    const auto pool = MakeCaretPool(gpu);
    DxUi::EmbeddedHost view;
    RequireSucceeded(view.Attach(pool, StoppedTestClock()), "attach the caret view");
    auto& host          = view.Controls();
    auto theme          = host.GetTheme();
    theme.reducedMotion = false;
    host.SetTheme(theme);
    host.DebugSetCaretBlinkSettingsForTest(Settings{530u, 0u});
    auto root   = std::make_unique<DxUi::Panel>();
    auto* field = root->AddChild<DxUi::TextField>(L"alpha");
    field->SetBounds(D2D1::RectF(8.0f, 8.0f, 208.0f, 40.0f));
    auto* combo = root->AddChild<DxUi::ComboBox>();
    combo->SetEditable(true);
    combo->SetItems({DxUi::ComboBox::Item{L"one", L"One"}, DxUi::ComboBox::Item{L"two", L"Two"}});
    combo->SetBounds(D2D1::RectF(8.0f, 48.0f, 208.0f, 80.0f));
    SizeChildren(*root);
    host.SetRoot(std::move(root));
    RequireSucceeded(view.Prepare(320u, 240u), "prepare the caret view");
    static_cast<void>(view.AdvanceAnimation(10'000u)); // the root's discovery tick

    host.SetFocusControl(field, false);
    Require(AnimationRequested(view.GetAnimationRequest()), "a focused caret asks for its deadline");
    static_cast<void>(view.AdvanceAnimation(10'001u)); // baseline
    static_cast<void>(view.AdvanceAnimation(10'001u + 530u));
    Require(! field->DebugIsCaretVisible(), "the caret blinks off after one period");

    view.SetVisible(false);
    Require(! AnimationRequested(view.GetAnimationRequest()), "a hidden view asks for nothing");
    view.SetVisible(true);
    Require(field->DebugIsCaretVisible(), "on show the caret is visible at once");
    Require(WakeIsDeadlineAt(host, host.GetAnimationTickMs() + 530u) && host.DebugGetTickRequestCountForTest() == 1u,
            "on show the phase restarts: the next flip is one period later");

    // An open popup of the same host keeps the focus on the editable box, so its caret keeps blinking.
    host.SetFocusControl(combo, false);
    const D2D1_RECT_F bounds = combo->GetBounds();
    static_cast<void>(combo->OnMouseDown(host, D2D1::Point2F(bounds.right - 8.0f, (bounds.top + bounds.bottom) * 0.5f), false, 0u));
    Require(combo->IsPopupOpen() && combo->HasFocus() && host.DebugGetFrameScheduleWakeForTest().kind == 2u && host.DebugGetTickRequestCountForTest() == 1u,
            "an open popup keeps the focused box's caret blinking, with one deadline");
    host.SetFocusControl(field, false);
    Require(host.DebugGetTickRequestCountForTest() == 1u, "focus moved from the box with its popup to the field: one deadline");

    // A caret owner hidden while focused: its deadline is held without a wake (the frame schedule skips a hidden control).
    field->SetVisible(false);
    Require(WakeIsNone(host) && field->DebugIsCaretVisible(), "a caret owner hidden while focused wakes nothing");
    field->SetVisible(true);
    host.SetFocusControl(nullptr, false);
    Require(WakeIsNone(host) && ! AnimationRequested(view.GetAnimationRequest()), "with no focus the view asks for nothing");
}

// Native: the window's own WM_SETTINGCHANGE re-reads the settings, and a device loss leaves the caret's phase and its
// single deadline alone (the clock owns no device resource).
void TestCaretClockNativeSettingChangeAndDeviceLoss()
{
    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host = window.Host();
    EnableMotionForTest(host);
    // A long period, on the real clock: no flip falls inside the short pumps below.
    host.DebugSetCaretBlinkSettingsForTest(Settings{60'000u, 0u});
    auto root   = std::make_unique<DxUi::Panel>();
    auto* field = root->AddChild<DxUi::TextField>(L"alpha");
    field->SetBounds(D2D1::RectF(8.0f, 8.0f, 208.0f, 40.0f));
    SizeChildren(*root);
    host.SetRoot(std::move(root));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages(100u);
    host.SetFocusControl(field, false);
    const auto armed = host.DebugGetFrameScheduleWakeForTest();
    Require(armed.kind == 2u && host.DebugGetTickRequestCountForTest() == 1u, "the focused caret holds one deadline");

    host.DebugSimulateDeviceLoss();
    window.PumpMessages(100u);
    Require(WakeIsDeadlineAt(host, armed.dueTickMs) && host.DebugGetTickRequestCountForTest() == 1u && field->DebugIsCaretVisible(),
            "a device loss leaves the caret's phase and deadline alone");

    host.DebugSetCaretBlinkSettingsForTest(Settings{INFINITE, 0u});
    static_cast<void>(SendMessageW(window.Hwnd(), WM_SETTINGCHANGE, 0u, 0));
    Require(WakeIsNone(host) && field->DebugIsCaretVisible(), "the window's WM_SETTINGCHANGE applies a steady caret at once");
    host.DebugSetCaretBlinkSettingsForTest(Settings{60'000u, 0u});
    static_cast<void>(SendMessageW(window.Hwnd(), WM_SETTINGCHANGE, 0u, 0));
    const auto rearmed = host.DebugGetFrameScheduleWakeForTest();
    Require(rearmed.kind == 2u && rearmed.dueTickMs >= armed.dueTickMs, "and a period again blinks from now");
    host.SetFocusControl(nullptr, false);
    Require(WakeIsNone(host), "blur leaves nothing armed");
}

namespace
{
// Pumps this thread's messages for `durationMs` of real time, blocking between them (no busy polling).
void PumpCaretFor(uint64_t durationMs)
{
    const uint64_t end = GetTickCount64() + durationMs;
    for (uint64_t now = GetTickCount64(); now < end; now = GetTickCount64())
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
            static_cast<void>(DispatchQueuedMessageForTest(message));
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, static_cast<DWORD>(end - now), QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
}

// Pumps this thread's messages until `reached` holds, with no wall-time bound of its own: how long a busy machine takes to
// reach a condition is not what is under test, so a condition that never comes ends the run at the test watchdog.
template <class Condition> void PumpCaretUntil(Condition&& reached)
{
    while (! reached())
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
            static_cast<void>(DispatchQueuedMessageForTest(message));
        if (reached())
            return;
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, 50u, QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
}
} // namespace

// W3.2 review F2: a host hidden only through an ancestor (a hidden parent, the parent's redraw block) gets no message when
// the ancestor shows again; the first wake that finds it visible resumes as a show does, so a focused caret restarts its
// phase (and its caret timeout) instead of firing the held flip from the old anchor. Blink 100 ms, timeout 600 ms: about
// six flips after the restart, whether the hide was shorter or longer than the timeout.
void TestCaretClockRestartsWhenAnAncestorShowsAgain()
{
    enum class Hide
    {
        ParentHiddenShort,
        ParentHiddenLong,
        ParentRedrawBlock,
    };
    for (const Hide hide : {Hide::ParentHiddenShort, Hide::ParentHiddenLong, Hide::ParentRedrawBlock})
    {
        wil::unique_hwnd parent(CreateWindowExW(
            WS_EX_NOACTIVATE, L"STATIC", L"caret clock parent", WS_OVERLAPPED, -32000, -32000, 360, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
        Require(parent != nullptr, "the parent window is created");
        AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
        SetWindowLongPtrW(window.Hwnd(), GWL_STYLE, WS_CHILD | WS_VISIBLE);
        Require(SetParent(window.Hwnd(), parent.get()) != nullptr || GetLastError() == ERROR_SUCCESS, "the host window becomes a child");
        SetWindowPos(window.Hwnd(), nullptr, 0, 0, 320, 200, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        auto& host = window.Host();
        EnableMotionForTest(host);
        host.DebugSetCaretBlinkSettingsForTest(Settings{100u, 600u});
        auto root   = std::make_unique<DxUi::Panel>();
        auto* field = root->AddChild<DxUi::TextField>(L"caret");
        field->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
        SizeChildren(*root);
        host.SetRoot(std::move(root));
        ShowWindow(parent.get(), SW_SHOWNOACTIVATE);
        // The real-time lengths below (a settle, a hide of 300 ms or 1,500 ms against the 600 ms caret timeout and the host's
        // one-second look at an ancestor) are the scenario itself, not a bound on how fast the machine must be.
        PumpCaretFor(200u);
        host.SetFocusControl(field, false);
        PumpCaretFor(150u);
        if (hide == Hide::ParentRedrawBlock)
        {
            SendMessageW(parent.get(), WM_SETREDRAW, FALSE, 0);
            PumpCaretFor(1'500u);
            SendMessageW(parent.get(), WM_SETREDRAW, TRUE, 0);
        }
        else
        {
            ShowWindow(parent.get(), SW_HIDE);
            PumpCaretFor(hide == Hide::ParentHiddenShort ? 300u : 1'500u);
            ShowWindow(parent.get(), SW_SHOWNOACTIVATE);
        }
        // The ancestor's show reaches the host at its next look, then the restarted caret blinks until its 600 ms timeout. Both
        // reads wait on their condition, bounded only by the test watchdog.
        const WorkCounterScope shown;
        PumpCaretUntil([&shown] { return shown.Delta().frame.tickCalls >= 4u; });
        const uint64_t ticks = shown.Delta().frame.tickCalls;
        std::cout << "    caret after the ancestor is shown again (" << static_cast<int>(hide) << "): " << ticks << " Tick calls\n";
        Require(ticks >= 4u, "W3.2: a caret whose host was hidden through an ancestor restarts its blink when the ancestor shows again");
        PumpCaretUntil([&host] { return host.DebugIsCaretClockVisibleForTest() && WakeIsNone(host); });
        Require(host.DebugIsCaretClockVisibleForTest() && WakeIsNone(host), "and times out again, shown, with nothing armed");
        host.SetFocusControl(nullptr, false);
        SetParent(window.Hwnd(), nullptr);
    }
}

// W3.2 review F4: a focused caret owner that is disabled draws no blinking caret: its clock schedules nothing and
// invalidates nothing; enabled again while still focused, it blinks from then on.
void TestCaretClockDisabledWhileFocusedSchedulesNothing()
{
    for (int owner = 0; owner < 3; ++owner)
    {
        DxUi::WindowHost host;
        PrepareHost(host, Settings{530u, 5000u});
        auto root              = std::make_unique<DxUi::Panel>();
        DxUi::Control* control = nullptr;
        if (owner == 0)
            control = root->AddChild<DxUi::TextField>(L"alpha");
        else if (owner == 1)
        {
            auto* combo = root->AddChild<DxUi::ComboBox>();
            combo->SetEditable(true);
            combo->SetText(L"alpha");
            control = combo;
        }
        else
            control = root->AddChild<DxUi::RichTextEditor>(L"alpha");
        control->SetBounds(D2D1::RectF(8.0f, 8.0f, 300.0f, 180.0f));
        SizeChildren(*root);
        host.SetRoot(std::move(root));
        host.SetFocusControl(control, false);
        Require(WakeIsDeadlineAt(host, kOriginMs + 530u), "the focused caret holds its deadline");
        control->SetEnabled(false);
        uint64_t now       = kOriginMs;
        const SleepRun run = SleepUntilDeadlines(host, now, kOriginMs + 10'000u);
        Require(run.wakes == 0u && run.invalidations == 0u && WakeIsNone(host), "W3.2: a disabled focused caret owner wakes and invalidates nothing");
        control->SetEnabled(true);
        if (control->HasFocus())
            Require(WakeIsDeadlineAt(host, now + 530u), "W3.2: enabled again while focused, the caret blinks from now");
        else
            Require(WakeIsNone(host), "a caret owner that lost focus when disabled schedules nothing");
    }
}

// W3.2 review: a disabled ancestor. Control::IsEnabled is the control's own flag and nothing tells a descendant that an
// ancestor was disabled; every caret owner's paint tests only its own state, so it still draws its caret, and the clock
// keeps blinking it, consistently. The host drops the focus of a control below a disabled ancestor at its next message
// (interaction-state pruning), which stops the caret and leaves nothing armed.
void TestCaretClockDisabledAncestor()
{
    for (int owner = 0; owner < 3; ++owner)
    {
        DxUi::WindowHost host;
        PrepareHost(host, Settings{530u, 0u});
        auto root              = std::make_unique<DxUi::Panel>();
        auto* group            = root->AddChild<DxUi::Panel>();
        DxUi::Control* control = nullptr;
        if (owner == 0)
            control = group->AddChild<DxUi::TextField>(L"alpha");
        else if (owner == 1)
        {
            auto* combo = group->AddChild<DxUi::ComboBox>();
            combo->SetEditable(true);
            combo->SetText(L"alpha");
            control = combo;
        }
        else
            control = group->AddChild<DxUi::RichTextEditor>(L"alpha");
        root->SetBounds(D2D1::RectF(0.0f, 0.0f, 400.0f, 300.0f));
        group->SetBounds(D2D1::RectF(0.0f, 0.0f, 400.0f, 300.0f));
        control->SetBounds(D2D1::RectF(8.0f, 8.0f, 300.0f, 180.0f));
        host.SetRoot(std::move(root));
        host.SetFocusControl(control, false);
        group->SetEnabled(false);
        Require(control->IsEnabled() && control->HasFocus(), "an owner below a disabled ancestor keeps its own enabled state and, until a message, its focus");
        uint64_t now       = kOriginMs;
        const SleepRun run = SleepUntilDeadlines(host, now, kOriginMs + 2'000u);
        Require(run.wakes == 3u && run.invalidations == 3u, "W3.2: its caret, which paint still draws, keeps blinking");
        bool handled = false;
        static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0u, MAKELPARAM(390, 290), handled));
        Require(! control->HasFocus() && WakeIsNone(host), "W3.2: the next message drops the focus below the disabled ancestor and the caret stops");
    }
}

// W3.2 review: a caret owner with no area draws nothing, so its caret holds no deadline; given an area while focused, it
// shows its caret and blinks from then on; losing its area again withdraws the deadline.
void TestCaretClockOwnerWithoutAreaSchedulesNothing()
{
    for (int owner = 0; owner < 3; ++owner)
    {
        DxUi::WindowHost host;
        PrepareHost(host, Settings{530u, 0u});
        auto root              = std::make_unique<DxUi::Panel>();
        DxUi::Control* control = nullptr;
        if (owner == 0)
            control = root->AddChild<DxUi::TextField>(L"alpha");
        else if (owner == 1)
        {
            auto* combo = root->AddChild<DxUi::ComboBox>();
            combo->SetEditable(true);
            combo->SetText(L"alpha");
            control = combo;
        }
        else
            control = root->AddChild<DxUi::RichTextEditor>(L"alpha");
        host.SetRoot(std::move(root));
        host.SetFocusControl(control, false);
        uint64_t now = kOriginMs;
        Require(control->HasFocus() && WakeIsNone(host), "W3.2: a focused caret owner with no area schedules nothing");
        Require(SleepUntilDeadlines(host, now, kOriginMs + 5'000u).wakes == 0u, "and wakes nothing");
        control->SetBounds(D2D1::RectF(8.0f, 8.0f, 300.0f, 40.0f));
        Require(WakeIsDeadlineAt(host, now + 530u), "W3.2: given an area, the focused caret blinks from now");
        const SleepRun run = SleepUntilDeadlines(host, now, now + 1'100u);
        Require(run.wakes == 2u && run.invalidations == 2u, "and flips once per period");
        control->SetBounds(D2D1::RectF(8.0f, 8.0f, 8.0f, 40.0f));
        Require(WakeIsNone(host), "W3.2: losing its area withdraws the caret's deadline");
    }
}
