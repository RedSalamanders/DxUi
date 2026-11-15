#pragma once
// FX14 tooltip path (plan W0.4): the W1.6 hover path (Embedded.Tests.TooltipHoverPath.h). The pointer enters one
// tooltip-bearing Label, makes 50 moves inside it during the hover delay, the fixture clock passes the show delay, the
// pointer makes the same 50 moves while the tooltip is shown, rests until the 5 s display lifetime hides it, and leaves.
// Proves G1, G2.
//
// The show delay passes on the frozen fixture clock: the show step ticks once at +1 ms (the embedded host's first tick
// after a pause only sets its clock's baseline) and once at +10 s; the hide step ticks once at the hide deadline, 5 s after
// the show. Show and hide are deadlines of the host's frame schedule (W3.3): the tooltip is ticked at those two and never
// between them, and in real time the native host's timer wakes only for them (TestTooltipDeadlinesWakeOnlyAtShowAndHideInRealTime in DxUi.Tests.Tooltip.cpp).
//
// On a WindowHost the test window sits off every monitor, so the system would post WM_MOUSELEAVE right after each move; the
// window runner drops it (only the leave step's own WM_MOUSELEAVE reaches the host), so pending moves measure what a cursor
// resting over the Label costs. Each window move is preceded by the cursor query Windows' WM_SETCURSOR makes at the same
// point (W2.9), so a window step counts what a real cursor move costs: one hit test, reused by the move.
#include "Embedded.Tests.PerformanceFixture.h"
#include "Embedded.Tests.TooltipHoverPath.h"

namespace PerformanceFixtures::Fx14
{
inline constexpr std::array kBudgets{std::string_view("G1"), std::string_view("G2")};
inline constexpr std::wstring_view kTooltipText = L"Tooltip path help";
inline constexpr size_t kWarm                   = 0u;
inline constexpr size_t kEnter                  = 1u;
inline constexpr size_t kPending                = 2u;
inline constexpr size_t kShow                   = 3u;
inline constexpr size_t kShown                  = 4u;
inline constexpr size_t kHide                   = 5u;
inline constexpr size_t kLeave                  = 6u;
inline constexpr uint64_t kDisplayLifetimeMs    = 5'000u;

inline constexpr std::array kVariants{Variant{"default", 320u, 180u, 96.0f, false}};
inline constexpr std::array kPhases{
    Phase{"warm", 2u, PhaseKind::Warmup, "nothing changes: the first frames lay out the Label"},
    Phase{"enter", 1u, PhaseKind::Measured, "the pointer enters the tooltip-bearing Label: the tooltip is scheduled"},
    Phase{"pending", kTooltipHoverPathMoves, PhaseKind::Measured, "50 moves inside the Label during the hover delay"},
    Phase{"show", 1u, PhaseKind::Measured, "the fixture clock passes the show delay: the tooltip shows at the last pointer origin"},
    Phase{"shown", kTooltipHoverPathMoves, PhaseKind::Measured, "the same 50 moves inside the Label while the tooltip is shown"},
    Phase{"hide", 1u, PhaseKind::Measured, "the pointer rests until the fixture clock reaches the 5 s display lifetime: the tooltip hides"},
    Phase{"leave", 1u, PhaseKind::Measured, "the pointer leaves the Label for empty space"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx14.h"),
                                     std::string_view("Tests/Embedded/Embedded.Tests.TooltipHoverPath.h")};

class Scene final : public Scenario
{
public:
    HRESULT Build(Context& context) override
    {
        context.controls.SetRoot(MakeTooltipHoverPathRoot(kTooltipText));
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        const auto move = [&](POINT pixel)
        {
            // Windows sends WM_SETCURSOR before each mouse move, at the same point; the window runner sends only the move, so
            // the step makes the cursor query that message runs (W2.9). An embedded host has no cursor query.
            if (context.host == Host::Window)
                static_cast<void>(context.controls.DebugResolveCursorKindForPoint(
                    D2D1::Point2F(context.controls.PixelsToDip(static_cast<float>(pixel.x)), context.controls.PixelsToDip(static_cast<float>(pixel.y)))));
            static_cast<void>(context.port.Pointer({DxUi::PointerAction::Move, static_cast<float>(pixel.x), static_cast<float>(pixel.y)}));
        };
        switch (phase)
        {
            case kEnter: move(TooltipHoverPathPixel(0u)); break;
            case kPending:
            case kShown: move(TooltipHoverPathPixel(step + 1u)); break;
            case kShow:
                context.port.AdvanceTime(1u);
                context.port.AdvanceTime(10'000u);
                break;
            case kHide: context.port.AdvanceTime(kDisplayLifetimeMs); break;
            case kLeave: move(POINT{static_cast<LONG>(context.variant.width) - 4, static_cast<LONG>(context.variant.height) - 4}); break;
            default: break;
        }
    }
};

inline constexpr Definition kDefinition{
    .id         = "FX14",
    .title      = "Tooltip path",
    .scenario   = "The pointer moving inside one tooltip-bearing control during the hover delay and while shown (the W1.6 path: 50 moves each), "
                  "with the show delay and the 5 s display lifetime passed on the frozen fixture clock. Proves G1, G2.",
    .budgets    = kBudgets,
    .hosts      = kBothHosts,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "shown",
    .sources    = kSources,
    .create     = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
};
} // namespace PerformanceFixtures::Fx14
