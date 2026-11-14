#pragma once
// The framework's own fixture, outside the catalog: a small scene on both hosts that exercises every runner path (phases,
// pointer input, a value change, the fixture clock) so the runners and the determinism comparator are tested before any
// plan fixture runs on them.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::SelfTest
{
inline constexpr std::array kVariants{Variant{"default", 320u, 200u, 96.0f, false}};
inline constexpr size_t kWarm  = 0u;
inline constexpr size_t kClean = 1u;
inline constexpr size_t kHover = 2u;
inline constexpr size_t kText  = 3u;
inline constexpr size_t kTime  = 4u;
inline constexpr std::array kPhases{
    Phase{"warm", 2u, PhaseKind::Warmup, "nothing changes: the first frames create the host's resources"},
    Phase{"clean", 3u, PhaseKind::Measured, "nothing changes"},
    Phase{"hover", 4u, PhaseKind::Measured, "the pointer moves onto the button on even steps and off it on odd steps"},
    Phase{"text", 3u, PhaseKind::Measured, "the label's text changes"},
    Phase{"time", 2u, PhaseKind::Measured, "the fixture clock advances 16 ms"},
};
// Root panel, label, button: three visible controls.
inline constexpr uint64_t kVisibleControls = 3u;

class Scene final : public Scenario
{
public:
    HRESULT Build(Context& context) override
    {
        auto root = std::make_unique<DxUi::Panel>();
        label_    = root->AddChild<DxUi::Label>(L"Value 0");
        label_->SetBounds(D2D1::RectF(8.0f, 8.0f, 160.0f, 32.0f));
        root->AddChild<DxUi::Button>(L"Hover")->SetBounds(D2D1::RectF(8.0f, 48.0f, 120.0f, 80.0f));
        context.controls.SetRoot(std::move(root));
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        const float scale = context.variant.dpi / 96.0f;
        switch (phase)
        {
            case kHover:
            {
                const float x = (step % 2u == 0u ? 60.0f : 200.0f) * scale;
                const float y = (step % 2u == 0u ? 64.0f : 150.0f) * scale;
                static_cast<void>(context.port.Pointer({DxUi::PointerAction::Move, x, y}));
                break;
            }
            case kText: label_->SetText(L"Value " + std::to_wstring(step + 1u)); break;
            case kTime: context.port.AdvanceTime(16u); break;
            default: break;
        }
    }

private:
    DxUi::Label* label_ = nullptr;
};

inline constexpr Definition kDefinition{
    .id         = "SELFTEST",
    .title      = "Performance fixture framework self-test",
    .scenario   = "A panel with a label and a button; phases exercise frames, pointer input, a text change and the fixture clock.",
    .hosts      = kBothHosts,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "text",
    .sources    = {},
    .create     = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
};
} // namespace PerformanceFixtures::SelfTest
