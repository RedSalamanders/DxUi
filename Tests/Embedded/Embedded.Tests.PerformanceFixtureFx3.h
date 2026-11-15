#pragma once
// FX3 Grid (plan W0.4): the 2,048-row, 4-column Grid of the W0.8 scroll scenario (Controls.Tests.GridScrollScenario.h:
// 24-DIP rows, 28-DIP header, 72-DIP columns, LargeGridModel) filling the view, driven by 100 wheel notches, 60 fractional
// touchpad deltas, 30 column-resize steps and 200 arrow keys; then the grouped model with a select-all. Proves G3, G8, G13
// (cells).
//
// Embedded host only: on a WindowHost the 422 steps cost about 170 s for the suites' two Debug runs (each WM_PAINT repaints
// and presents the whole window), against about 9 s embedded. Native gates on this workload use the W0.8 scroll scenario,
// whose geometry this fixture mirrors.
//
// The grouped scene is 20,000 rows in 1,000 groups in the default variant, which the suites run twice per host; the
// receipt-only "grouped-full" variant is the plan's 200,000 rows in 10,000 groups. Wheel notches scroll 3 lines: the scene pins
// the lines-per-notch setting through DebugSetWheelScrollLinesForTest (W1.2) and returns it to the system's at teardown.
#include "Embedded.Tests.PerformanceFixture.h"
#include "Embedded.Tests.PerformanceGridModels.h"

namespace PerformanceFixtures::Fx3
{
inline constexpr std::array kBudgets{std::string_view("G3"), std::string_view("G8"), std::string_view("G13")};
inline constexpr size_t kRows          = 2048u;
inline constexpr size_t kColumns       = 4u;
inline constexpr float kColumnWidth    = 72.0f;
inline constexpr float kRowHeightDip   = 24.0f;
inline constexpr float kHeaderHeight   = 28.0f;
inline constexpr float kTouchpadDelta  = 40.0f; // a third of a notch
inline constexpr size_t kResizeSteps   = 30u;
inline constexpr float kResizeStepDip  = 2.0f;
inline constexpr size_t kWarm          = 0u;
inline constexpr size_t kWheel         = 1u;
inline constexpr size_t kTouchpad      = 2u;
inline constexpr size_t kResize        = 3u;
inline constexpr size_t kArrows        = 4u;
inline constexpr size_t kGroupedWheel  = 5u;
inline constexpr size_t kSelectAll     = 6u;
inline constexpr size_t kSelectedArrow = 7u;

inline constexpr std::array kVariants{
    Variant{"default", 800u, 600u, 96.0f, false},
    Variant{"grouped-full", 800u, 600u, 96.0f, false, true},
    // The hardware attribution receipt (W0.7) at 1920x1080 and 3840x2160: the Grid fills the larger view.
    Variant{"fhd", 1920u, 1080u, 96.0f, true, true},
    Variant{"uhd-192dpi", 3840u, 2160u, 192.0f, true, true},
};
inline constexpr std::array kPhases{
    Phase{"warm", 2u, PhaseKind::Warmup, "nothing changes: the first frames lay out the Grid"},
    Phase{"wheel", 100u, PhaseKind::Measured, "one wheel notch toward the end over the Grid's centre"},
    Phase{"touchpad", 60u, PhaseKind::Measured, "a fractional wheel delta of 40 (a third of a notch) toward the end"},
    Phase{"resize",
          kResizeSteps,
          PhaseKind::Measured,
          "step 0 hovers and presses on the first column's divider, steps 1-28 drag it 2 DIP right, step 29 releases"},
    Phase{"arrows", 200u, PhaseKind::Measured, "step 0 focuses the Grid and its first row; every step presses and releases Down"},
    Phase{"grouped-wheel", 20u, PhaseKind::Measured, "step 0 swaps in the grouped model; every step scrolls one wheel notch toward the end"},
    Phase{"select-all", 1u, PhaseKind::Measured, "select-all over the grouped model (Extended selection)"},
    Phase{"selected-arrow", 10u, PhaseKind::Measured, "Down with every row selected: each key replaces the whole selection"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx3.h"),
                                     std::string_view("Tests/Embedded/Embedded.Tests.PerformanceGridModels.h")};

class Scene final : public Scenario
{
public:
    ~Scene() override
    {
        DxUi::DebugSetWheelScrollLinesForTest(std::nullopt);
    }
    HRESULT Build(Context& context) override
    {
        DxUi::DebugSetWheelScrollLinesForTest(3u);
        const bool full = context.variant.name == "grouped-full";
        grouped_.emplace(full ? 200'000u : 20'000u, full ? 10'000u : 1'000u, kColumns, kColumnWidth);
        auto root = std::make_unique<DxUi::Panel>();
        grid_     = root->AddChild<DxUi::Grid>();
        grid_->SetRowHeightDip(kRowHeightDip);
        grid_->SetHeaderHeightDip(kHeaderHeight);
        grid_->SetSelectionMode(DxUi::GridSelectionMode::Extended);
        grid_->SetModel(&rows_);
        const float scale = context.variant.dpi / 96.0f;
        const D2D1_RECT_F bounds =
            D2D1::RectF(0.0f, 0.0f, static_cast<float>(context.variant.width) / scale, static_cast<float>(context.variant.height) / scale);
        root->SetBounds(bounds);
        grid_->SetBounds(bounds);
        context.controls.SetRoot(std::move(root));
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        const float scale   = context.variant.dpi / 96.0f;
        const float centerX = static_cast<float>(context.variant.width) / 2.0f;
        const float centerY = static_cast<float>(context.variant.height) / 2.0f;
        const auto wheel    = [&](float delta)
        { static_cast<void>(context.port.Pointer({.action = DxUi::PointerAction::Wheel, .xPixels = centerX, .yPixels = centerY, .wheelDelta = delta})); };
        const auto key = [&](UINT virtualKey)
        {
            static_cast<void>(context.port.Key(virtualKey, true));
            static_cast<void>(context.port.Key(virtualKey, false));
        };
        switch (phase)
        {
            case kWheel: wheel(-120.0f); break;
            case kTouchpad: wheel(-kTouchpadDelta); break;
            case kResize:
            {
                const float x     = (kColumnWidth - 1.0f + kResizeStepDip * static_cast<float>(step == 0u ? 0u : step - 1u)) * scale;
                const float y     = kHeaderHeight / 2.0f * scale;
                const auto action = step == 0u ? DxUi::PointerAction::Down : (step + 1u == kResizeSteps ? DxUi::PointerAction::Up : DxUi::PointerAction::Move);
                if (step == 0u)
                    static_cast<void>(context.port.Pointer({DxUi::PointerAction::Move, x, y})); // hover the divider first
                static_cast<void>(context.port.Pointer({action, x, y}));
                break;
            }
            case kArrows:
            case kSelectedArrow:
                if (phase == kArrows && step == 0u)
                {
                    context.controls.SetFocusControl(grid_, false);
                    static_cast<void>(grid_->RequestFocusRow(0u));
                }
                key(VK_DOWN);
                break;
            case kGroupedWheel:
                if (step == 0u)
                    grid_->SetModel(&*grouped_);
                wheel(-120.0f);
                break;
            case kSelectAll: static_cast<void>(grid_->OnSelectAll(context.controls)); break;
            default: break;
        }
    }

private:
    LargeGridModel rows_{kRows, kColumns, kColumnWidth};
    std::optional<UniformGroupedGridModel> grouped_;
    DxUi::Grid* grid_ = nullptr;
};

inline constexpr Definition kDefinition{
    .id         = "FX3",
    .title      = "Grid scrolling, resizing and keys",
    .scenario   = "A Grid of 2,048 rows: 100 wheel notches, 60 fractional touchpad deltas, 30 column-resize steps, 200 arrow keys; then a grouped "
                  "model with a select-all (default variant 20,000 rows in 1,000 groups; receipt-only grouped-full 200,000 rows in 10,000 "
                  "groups). Proves G3, G8, G13 (cells).",
    .budgets    = kBudgets,
    .hosts      = kEmbedded,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "wheel",
    .sources    = kSources,
    .create     = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
};
} // namespace PerformanceFixtures::Fx3
