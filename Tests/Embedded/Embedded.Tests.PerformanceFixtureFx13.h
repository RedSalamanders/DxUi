#pragma once
// FX13 Whole application (plan W0.4): 60 s idle with nothing armed and with a focused caret, then hidden; and, as a
// receipt-only real-time measurement outside this definition, cold start to the first presented frame
// (Controls.Tests.PerformanceFixtureColdStart.cpp, which builds the same application on a WindowHost in a fresh process).
//
// What is simulated: time is the frozen fixture clock. An idle step advances it by one second and, exactly when the view
// asks for animation (EmbeddedHost::NeedsAnimation), delivers one AdvanceAnimation tick at the new time, as an application
// that honours NeedsAnimation does; that tick is the step's wake (wakes.embeddedAdvance) and walks the tree
// (frame.treeVisitsTick). So an idle step records at most one wake, and 60 steps are 60 simulated seconds. A real native
// host wakes from its animation dispatcher's SetTimer(8 ms), which Windows raises to the 10 ms USER_TIMER_MINIMUM, for as
// long as anything is armed: up to 100 wakes per armed second, which the 1-second steps do not show. The caret-period phase
// replays one second at that 10 ms period to show the per-second count. A step with nothing armed records 0 wakes on both.
//
// Embedded host only: the window runner's clock delivers DebugAnimationTickForTest on every AdvanceTime whether or not
// anything is armed, so it cannot show an idle window waking nothing. The memory fields come from the embedded runner's
// W0.3 samples: tracked live bytes and retained statistics after each phase (idle) and after the view is hidden (hidden is
// how an embedded view is minimized). Video memory is read on a hardware adapter only, hence the hardware-only variant.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx13
{
inline constexpr size_t kIdleSeconds     = 60u;
inline constexpr uint64_t kIdleStepMs    = 1'000u;
inline constexpr size_t kTimerSteps      = 100u;
inline constexpr uint64_t kTimerPeriodMs = 10u; // SetTimer(8 ms) raised to USER_TIMER_MINIMUM

inline constexpr std::array kBudgets{std::string_view("G1"), std::string_view("G13")};
inline constexpr std::array kVariants{
    Variant{"default", 1920u, 1080u, 96.0f, false},
    // The same 1080p application on a hardware adapter: the hidden sample then carries local video memory (G13, minimized).
    Variant{"minimized-hardware", 1920u, 1080u, 96.0f, true},
};
inline constexpr size_t kNothingArmed = 1u;
inline constexpr size_t kCaret        = 2u;
inline constexpr size_t kCaretPeriod  = 3u;
inline constexpr std::array kPhases{
    Phase{"warm", 2u, PhaseKind::Warmup, "the first frame and one more: first-use resources settle"},
    Phase{"idle-nothing-armed",
          kIdleSeconds,
          PhaseKind::Measured,
          "nothing focused or animating: each step advances the fixture clock by 1 s, ticking only if the view asks for animation"},
    Phase{"idle-focused-caret",
          kIdleSeconds,
          PhaseKind::Measured,
          "step 0 gives the search field logical focus (its caret); each step advances the fixture clock by 1 s, ticking only if "
          "the view asks for animation"},
    Phase{"caret-timer-period",
          kTimerSteps,
          PhaseKind::Measured,
          "the focused caret for one more second at the native dispatcher's 10 ms timer period: each step advances the clock by "
          "10 ms, ticking only if the view asks for animation"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx13.h")};

// The application: a title, a search field with three buttons, eight options, eight features, eight levels, four progress
// bars and a status line (34 controls under the root panel). Returns the search field. Shared with the cold-start
// measurement, which builds it on a WindowHost.
inline DxUi::TextField* BuildApplication(DxUi::ControlHost& host)
{
    auto root = std::make_unique<DxUi::Panel>();
    root->AddChild<DxUi::Label>(L"DxUi application")->SetBounds(D2D1::RectF(16.0f, 16.0f, 616.0f, 48.0f));
    auto* search = root->AddChild<DxUi::TextField>();
    search->SetBounds(D2D1::RectF(16.0f, 56.0f, 416.0f, 88.0f));
    search->SetAccessibleName(L"Search");
    const wchar_t* const buttons[] = {L"Find", L"Clear", L"Settings"};
    for (size_t index = 0u; index < std::size(buttons); ++index)
    {
        const float left = 432.0f + static_cast<float>(index) * 104.0f;
        root->AddChild<DxUi::Button>(buttons[index])->SetBounds(D2D1::RectF(left, 56.0f, left + 96.0f, 88.0f));
    }
    for (size_t index = 0u; index < 8u; ++index)
    {
        const float top = 104.0f + static_cast<float>(index) * 36.0f;
        root->AddChild<DxUi::Checkbox>(L"Option " + std::to_wstring(index + 1u))->SetBounds(D2D1::RectF(16.0f, top, 296.0f, top + 28.0f));
        auto* toggle = root->AddChild<DxUi::Toggle>(L"Feature " + std::to_wstring(index + 1u));
        toggle->SetBounds(D2D1::RectF(320.0f, top, 600.0f, top + 28.0f));
        auto* slider = root->AddChild<DxUi::Slider>();
        slider->SetBounds(D2D1::RectF(624.0f, top, 904.0f, top + 28.0f));
        slider->SetAccessibleName(L"Level " + std::to_wstring(index + 1u));
        slider->SetMinimum(0.0);
        slider->SetMaximum(100.0);
        slider->SetValue(static_cast<double>(index) * 12.5);
    }
    for (size_t index = 0u; index < 4u; ++index)
    {
        const float top = 408.0f + static_cast<float>(index) * 24.0f;
        auto* progress  = root->AddChild<DxUi::ProgressBar>();
        progress->SetBounds(D2D1::RectF(16.0f, top, 904.0f, top + 12.0f));
        progress->SetValue(0.2 + 0.2 * static_cast<double>(index));
    }
    root->AddChild<DxUi::Label>(L"Ready")->SetBounds(D2D1::RectF(16.0f, 512.0f, 904.0f, 536.0f));
    host.SetRoot(std::move(root));
    return search;
}

class Scene final : public Scenario
{
public:
    HRESULT Build(Context& context) override
    {
        search_ = BuildApplication(context.controls);
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        switch (phase)
        {
            case kNothingArmed: context.port.AdvanceTime(kIdleStepMs); break;
            case kCaret:
                if (step == 0u)
                    context.controls.SetFocusControl(search_, false);
                context.port.AdvanceTime(kIdleStepMs);
                break;
            case kCaretPeriod: context.port.AdvanceTime(kTimerPeriodMs); break;
            default: break;
        }
    }

private:
    DxUi::TextField* search_ = nullptr;
};

inline constexpr Definition kDefinition{
    .id         = "FX13",
    .title      = "Whole application",
    .scenario   = "A 34-control application (title, search field and buttons, options, features, levels, progress, status) on a 1920x1080 "
                  "offscreen EmbeddedHost, reduced motion: 60 simulated seconds idle with nothing armed, 60 with the search field's caret, "
                  "one more second at the native 10 ms timer period, then hidden. Proves G1 (nothing armed) and G13 (idle and hidden); "
                  "cold start to the first presented frame is a separate receipt-only measurement.",
    .budgets    = kBudgets,
    .hosts      = kEmbedded,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "idle-focused-caret",
    .sources    = kSources,
    .create     = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
};
} // namespace PerformanceFixtures::Fx13
