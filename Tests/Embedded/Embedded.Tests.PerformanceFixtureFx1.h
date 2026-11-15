#pragma once
// FX1 idle caret (plan W0.4): a focused editable single-line TextField, then a focused read-only one, then a focused
// 64 Ki-unit multiline TextField, each left idle for 10 s of the frozen fixture clock. Proves G1; with FX5, G4. Motion is on and
// the caret settings are Windows' defaults (530 ms, 5 s timeout), pinned (W3.2): an editable caret blinks for 5 s after focus,
// then stays shown with nothing armed; the read-only caret is steady.
//
// What is simulated: every idle step advances the fixture clock by 100 ms and delivers one animation tick when the host has
// animation pending (embedded: EmbeddedHost::AdvanceAnimation, counted as wakes.embeddedAdvance; window:
// DebugAnimationTickForTest, which counts no wake), then renders one frame. So a step whose counters show a tick walk
// (frame.treeVisitsTick) or an embedded advance is a step in which something kept the clock armed. Real time adds what the
// fixture cannot: a native host's AnimationDispatcher timer fires every 8 ms (10 ms after Windows' minimum), so an armed
// window receives about 1,000 timer wakes in 10 s where this fixture delivers 100 ticks; wakes.animationTimer stays 0 here
// because no WM_TIMER is ever dispatched.
#include "Embedded.Tests.PerformanceFixture.h"

#include <string>

namespace PerformanceFixtures::Fx1
{
inline constexpr std::array kBudgets{std::string_view("G1"), std::string_view("G4")};
inline constexpr size_t kIdleSteps          = 100u; // 10 s at 100 ms per step
inline constexpr uint64_t kStepMs           = 100u;
inline constexpr size_t kMultilineUnits     = 65536u;
inline constexpr size_t kMultilineIdleSteps = 20u; // 10 s at 500 ms per step: each 64 Ki-unit paint is expensive
inline constexpr uint64_t kMultilineStepMs  = 500u;
inline constexpr size_t kWarm               = 0u;
inline constexpr size_t kEditable           = 1u;
inline constexpr size_t kReadOnly           = 2u;
inline constexpr size_t kMultiline          = 3u;
// Windows' defaults: a 530 ms blink phase, and blinking stops 5 s after the latest input with the caret shown (W3.2).
inline constexpr DxUi::ControlHost::CaretBlinkSettingsForTest kCaretSettings{530u, 5000u};

inline constexpr std::array kVariants{Variant{"default", 800u, 600u, 96.0f, false}};
inline constexpr std::array kPhases{
    Phase{"warm", 2u, PhaseKind::Warmup, "step 0 focuses the editable field (no native focus transfer); first frames"},
    Phase{"editable", kIdleSteps, PhaseKind::Measured, "editable single-line field focused and idle: 100 ms of fixture clock per step, 10 s"},
    Phase{"readonly", kIdleSteps, PhaseKind::Measured, "step 0 focuses the read-only single-line field; idle at 100 ms per step, 10 s"},
    Phase{"multiline", kMultilineIdleSteps, PhaseKind::Measured, "step 0 focuses the 64 Ki-unit multiline field; idle at 500 ms per step, 10 s"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx1.h")};

// 64 Ki UTF-16 units of prose in lines of 64 units (63 letters and spaces, then a line feed).
[[nodiscard]] inline std::wstring MultilineText()
{
    std::wstring text;
    text.reserve(kMultilineUnits);
    constexpr std::wstring_view kWords = L"the quick brown fox jumps over the lazy dog ";
    while (text.size() < kMultilineUnits)
    {
        const size_t column = text.size() % 64u;
        text.push_back(column == 63u ? L'\n' : kWords[text.size() % kWords.size()]);
    }
    return text;
}

class Scene final : public Scenario
{
public:
    HRESULT Build(Context& context) override
    {
        // The caret follows the user's blink time and caret timeout: pinned to Windows' defaults through the host's test seam,
        // never read from the desktop that runs the fixture. Motion is on (the definition), so the editable caret blinks.
        context.controls.DebugSetCaretBlinkSettingsForTest(kCaretSettings);
        auto root = std::make_unique<DxUi::Panel>();
        editable_ = root->AddChild<DxUi::TextField>();
        editable_->SetText(L"Editable field");
        editable_->SetBounds(D2D1::RectF(16.0f, 16.0f, 336.0f, 48.0f));
        readOnly_ = root->AddChild<DxUi::TextField>();
        readOnly_->SetText(L"Read-only field");
        readOnly_->SetReadOnly(true);
        readOnly_->SetBounds(D2D1::RectF(16.0f, 64.0f, 336.0f, 96.0f));
        multiline_ = root->AddChild<DxUi::TextField>();
        multiline_->SetMultiline(true);
        multiline_->SetText(MultilineText());
        multiline_->SetBounds(D2D1::RectF(16.0f, 112.0f, 776.0f, 584.0f));
        context.controls.SetRoot(std::move(root));
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        switch (phase)
        {
            case kWarm:
                if (step == 0u)
                    context.controls.SetFocusControl(editable_, false);
                break;
            case kEditable: context.port.AdvanceTime(kStepMs); break;
            case kReadOnly:
                if (step == 0u)
                    context.controls.SetFocusControl(readOnly_, false);
                context.port.AdvanceTime(kStepMs);
                break;
            case kMultiline:
                if (step == 0u)
                    context.controls.SetFocusControl(multiline_, false);
                context.port.AdvanceTime(kMultilineStepMs);
                break;
            default: context.port.AdvanceTime(kStepMs); break;
        }
    }

private:
    DxUi::TextField* editable_  = nullptr;
    DxUi::TextField* readOnly_  = nullptr;
    DxUi::TextField* multiline_ = nullptr;
};

inline constexpr Definition kDefinition{
    .id            = "FX1",
    .title         = "Idle caret",
    .scenario      = "A focused single-line TextField, editable and read-only, for 10 s; plus a focused 64 Ki-unit multiline TextField. Proves G1; with "
                     "FX5, G4. The fixture clock advances 100 ms per step (500 ms on the multiline field) with one animation tick per step when "
                     "animation is pending; a native 8 ms timer would deliver about ten times as many wakes in real time. Motion on; caret "
                     "settings pinned to Windows' defaults (530 ms blink, 5 s caret timeout).",
    .budgets       = kBudgets,
    .hosts         = kBothHosts,
    .variants      = kVariants,
    .phases        = kPhases,
    .timedPhase    = "editable",
    .sources       = kSources,
    .reducedMotion = false,
    .create        = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
};
} // namespace PerformanceFixtures::Fx1
