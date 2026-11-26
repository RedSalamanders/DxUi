#pragma once
// FX5 ComplexUi: the existing 83-control scene, clean and dirty frames (plan W0.4). The scene is Samples/ComplexUi/ComplexUiScene.h,
// shared verbatim with the canonical benchmark, so on the embedded host it owns its EmbeddedHost. On the window host (W0.9: the
// presentation-mode comparison runs FX5 in both swap-chain modes) the same scene supplies its control tree to the host's own
// ControlHost and changes the same values. The definition still declares the embedded host only: no window receipt, determinism
// gate or timing run of FX5 exists, and the comparison (Tests/Controls/Controls.Tests.PresentationModes.cpp) is the one user.
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
    // The hardware attribution receipt (W0.7) at 1920x1080: 144 DPI scales the 1280x720 DIP scene to fill the view.
    Variant{"fhd-144dpi", 1920u, 1080u, 144.0f, true, true},
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
    HRESULT Build(Context& context) override
    {
        // Embedded: ComplexUiScene::Initialize built the tree and applied its reduced-motion theme. Window: the runner's host
        // has the fixture theme (dark, reduced motion, as the scene sets) and the scene supplies the same tree.
        if (context.host == Host::Window)
        {
            context.controls.SetRoot(scene_.BuildRoot());
            scene_.Apply(0u);
        }
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        // The window host invalidates through the controls' setters; only the embedded view needs MarkDirty.
        if (phase == 0u || phase == 2u)
        {
            const size_t frame = phase == 0u ? step : kWarmSteps + step;
            if (context.host == Host::Embedded)
                scene_.Update(frame);
            else
                scene_.Apply(frame);
        }
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
    .id       = "FX5",
    .title    = "ComplexUi: the existing 83-control scene, clean and dirty frames",
    .scenario = "Samples/ComplexUi/ComplexUiScene.h: 16 cards (label, toggle, slider, progress bar), a Tree and a Grid over a 1,000-row model, "
                "absolute layout, offscreen EmbeddedHost, reduced motion; 20 warm-up frames, 40 clean frames, 40 dirty frames, 20 hover frames.",
    .budgets  = kBudgets,
    // Embedded only for the receipts, the determinism gate and the timing harness; the window form below exists for the
    // presentation-mode comparison (WindowFixtureSession), which names the definition itself and runs no window receipt of FX5.
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
