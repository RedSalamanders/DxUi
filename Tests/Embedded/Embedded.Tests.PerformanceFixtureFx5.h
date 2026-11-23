#pragma once
// FX5 ComplexUi: the existing 83-control scene, clean and dirty frames (plan W0.4). The scene is Samples/ComplexUi/ComplexUiScene.h,
// shared verbatim with the canonical benchmark, so it owns its EmbeddedHost: this fixture runs on the embedded host only.
#include "ComplexUiScene.h"
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx5
{
inline constexpr size_t kWarmSteps  = 20u;
inline constexpr size_t kCleanSteps = 40u;
inline constexpr size_t kDirtySteps = 40u;
inline constexpr size_t kHoverSteps = 20u;
inline constexpr size_t kHover      = 3u;

// G13 (W9.1): every memory line reported as a counter; lines over budget are recorded exceptions (kG13Exceptions).
inline constexpr std::array kBudgets{std::string_view("G2"), std::string_view("G3"), std::string_view("G4"), std::string_view("G11"), std::string_view("G13")};
inline constexpr std::array kVariants{
    Variant{"default", 1280u, 720u, 96.0f, false},
    // G4 is judged here, on a hardware adapter only.
    Variant{"uhd-192dpi", 3840u, 2160u, 192.0f, true},
};
inline constexpr std::array kPhases{
    Phase{"warm", kWarmSteps, PhaseKind::Warmup, "ComplexUiScene::Update(n): first-use resources and caches settle"},
    Phase{"clean", kCleanSteps, PhaseKind::Measured, "nothing changes: prepare and composite only"},
    Phase{"dirty", kDirtySteps, PhaseKind::Measured, "ComplexUiScene::Update(20 + n): 16 slider and progress values and the Grid's visible row change"},
    // G11 and G2 (W2.1): a frame in which nothing but hover state changes, so every string it draws was drawn before.
    Phase{"hover", kHoverSteps, PhaseKind::Measured, "the pointer moves between the first two cards' toggles: one hover change per frame"},
};
inline constexpr std::array kSources{std::string_view("Samples/ComplexUi/ComplexUiScene.h"),
                                     std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx5.h")};

class Scene final : public Scenario
{
public:
    DxUi::EmbeddedHost* AttachOwnEmbeddedHost(ID3D11Device* device, HRESULT& result) override
    {
        result = scene_.Initialize(device);
        return SUCCEEDED(result) ? &scene_.view : nullptr;
    }
    HRESULT Build(Context&) override
    {
        return S_OK; // ComplexUiScene::Initialize built the tree and applied its reduced-motion theme.
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        if (phase == 0u)
            scene_.Update(step);
        else if (phase == 2u)
            scene_.Update(kWarmSteps + step);
        else if (phase == kHover)
        {
            // The toggle of card 0 or card 1 (each card holds its label, toggle, slider and progress bar in that order).
            const DxUi::Control* root   = context.controls.GetRoot();
            const DxUi::Control* card   = root ? root->GetLogicalChild(step % 2u) : nullptr;
            const DxUi::Control* toggle = card ? card->GetLogicalChild(1u) : nullptr;
            if (! toggle)
                return;
            const D2D1_RECT_F bounds = toggle->GetBounds();
            const float scale        = context.variant.dpi / 96.0f;
            static_cast<void>(
                context.port.Pointer({DxUi::PointerAction::Move, (bounds.left + bounds.right) * 0.5f * scale, (bounds.top + bounds.bottom) * 0.5f * scale}));
        }
    }

private:
    ComplexUiScene scene_;
};

inline constexpr Definition kDefinition{
    .id         = "FX5",
    .title      = "ComplexUi: the existing 83-control scene, clean and dirty frames",
    .scenario   = "Samples/ComplexUi/ComplexUiScene.h: 16 cards (label, toggle, slider, progress bar), a Tree and a Grid over a 1,000-row model, "
                  "absolute layout, offscreen EmbeddedHost, reduced motion; 20 warm-up frames, 40 clean frames, 40 dirty frames, 20 hover frames.",
    .budgets    = kBudgets,
    .hosts      = kEmbedded,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "dirty",
    .sources    = kSources,
    .create     = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
    // ComplexUiScene::Update(n) cycles its values and visible row, so every dirty frame past step 40 changes the scene too.
    .timedPhaseRepeats = true,
};
} // namespace PerformanceFixtures::Fx5
