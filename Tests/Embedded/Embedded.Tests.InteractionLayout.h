#pragma once

#include "../../Samples/EmbeddedControls/EmbeddedScene.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include <cmath>
#include <cstdio>
#include <thread>

class InteractionLayoutAccessibilitySite final : public DxUi::EmbeddedAccessibilitySite
{
public:
    HRESULT Navigate(NavigateDirection, IRawElementProviderFragment** result) noexcept override
    {
        if (result)
            *result = nullptr;
        return S_FALSE;
    }
    HRESULT FragmentRoot(IRawElementProviderFragmentRoot** result) noexcept override
    {
        if (result)
            *result = nullptr;
        return S_FALSE;
    }
    HRESULT RequestFocus() noexcept override
    {
        return S_OK;
    }
};

class InteractionLayoutCaptureProbe final : public DxUi::Control
{
public:
    explicit InteractionLayoutCaptureProbe(DxUi::Control* sibling) noexcept : _sibling(sibling)
    {
    }
    void Paint(DxUi::ControlHost&) const override
    {
    }
    bool OnMouseDown(DxUi::ControlHost& host, D2D1_POINT_2F, bool, UINT) override
    {
        host.CaptureMouse(this);
        return true;
    }
    void OnCaptureLost(DxUi::ControlHost&) override
    {
        _sibling->SetBounds(D2D1::RectF(120, 120, 220, 180));
    }

private:
    DxUi::Control* _sibling;
};

static void CheckInteractionLayoutIndexed(bool value, const char* assertion, size_t index)
{
    char label[192]{};
    static_cast<void>(std::snprintf(label, sizeof(label), "interaction-layout %s #%zu", assertion, index));
    Check(value, label);
}

// Exercise the interaction-only preparation boundary independently of rendering and desktop state.
static void TestInteractionLayoutWithoutPaint(GraphicsFixture& gpu)
{
    constexpr UINT kWidth  = 480;
    constexpr UINT kHeight = 240;

    // An interaction acknowledgement cannot manufacture a layout before the first successful paint.
    {
        EmbeddedScene scene;
        Hr(scene.Initialize(gpu.device.get()), "interaction-layout initial scene attaches");
        Check(scene.view.PrepareInteraction(kWidth, kHeight) == S_FALSE, "interaction-layout initial acknowledgement is unavailable");
        Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 84}), "interaction-layout initial input waits for full preparation");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout first full preparation");
    }

    // Layer changes can relocate a live toggle several times before the host paints. Every tap must use the
    // caller's latest bounds while the previously rendered pixels remain dirty and all paint costs stay fixed.
    {
        EmbeddedScene scene;
        size_t requests = 0;
        Hr(scene.Initialize(gpu.device.get(), {&requests, [](void* p) noexcept { ++*static_cast<size_t*>(p); }}),
           "interaction-layout rapid-click scene attaches");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout rapid-click first paint");
        const auto painted = scene.view.GetStatistics();
        size_t clickNumber = 0;
        auto click         = [&](float x)
        {
            CheckInteractionLayoutIndexed(scene.view.DispatchPointer({DxUi::PointerAction::Down, x, 84}), "relocated toggle receives down", clickNumber);
            CheckInteractionLayoutIndexed(scene.view.DispatchPointer({DxUi::PointerAction::Up, x, 84}), "relocated toggle receives up", clickNumber);
            ++clickNumber;
        };
        click(100);
        Check(! scene.enabled, "interaction-layout first rapid click toggles off at original coordinates");
        for (int i = 0; i < 6; ++i)
        {
            const bool right = (i % 2) == 0;
            scene.toggle->SetBounds(right ? D2D1::RectF(370, 64, 460, 104) : D2D1::RectF(24, 64, 150, 104));
            CheckInteractionLayoutIndexed(scene.view.PrepareInteraction(kWidth, kHeight) == S_OK, "acknowledges relocated live bounds", static_cast<size_t>(i));
            CheckInteractionLayoutIndexed(scene.view.NeedsPreparation(), "acknowledgement leaves pixels dirty", static_cast<size_t>(i));
            click(right ? 410.0f : 80.0f);
            CheckInteractionLayoutIndexed(scene.enabled == ((i % 2) == 0), "rapid click toggles once at literal relocated coordinates", static_cast<size_t>(i));
        }
        const auto after = scene.view.GetStatistics();
        Check(after.preparations == painted.preparations, "interaction-layout rapid clicks perform no preparation");
        Check(after.composites == painted.composites, "interaction-layout rapid clicks perform no composition");
        Check(after.surfaceAllocations == painted.surfaceAllocations, "interaction-layout rapid clicks allocate no surfaces");
        Check(scene.view.NeedsPreparation(), "interaction-layout rapid-click sequence still requires paint");
        Check(scene.view.PrepareInteraction(kWidth, kHeight) == S_FALSE, "interaction-layout unchanged live bounds return S_FALSE");
    }

    // Moving, hiding or disabling the captured slider, and hiding its ancestor, cancels its draft at the
    // interaction boundary. The slider returns to its original value without waiting for paint.
    {
        EmbeddedScene scene;
        Hr(scene.Initialize(gpu.device.get()), "interaction-layout slider-cancel scene attaches");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout slider-cancel initial paint");
        const D2D1_RECT_F original = scene.slider->GetBounds();
        for (int mode = 0; mode < 3; ++mode)
        {
            Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout slider-cancel baseline paint");
            const double initial = scene.slider->GetValue();
            CheckInteractionLayoutIndexed(scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 184}), "slider draft starts", static_cast<size_t>(mode));
            CheckInteractionLayoutIndexed(scene.view.DispatchPointer({DxUi::PointerAction::Move, 340, 184}), "slider draft changes", static_cast<size_t>(mode));
            CheckInteractionLayoutIndexed(scene.slider->GetValue() != initial, "slider draft has a provisional value", static_cast<size_t>(mode));
            if (mode == 0)
                scene.slider->SetBounds(D2D1::RectF(24, 200, 440, 232));
            else if (mode == 1)
                scene.slider->SetVisible(false);
            else
                scene.slider->SetEnabled(false);
            CheckInteractionLayoutIndexed(
                scene.view.PrepareInteraction(kWidth, kHeight) == S_OK, "slider availability or bounds acknowledged", static_cast<size_t>(mode));
            CheckInteractionLayoutIndexed(
                scene.view.Controls().GetCapturedControl() == nullptr, "changed or unavailable slider releases capture", static_cast<size_t>(mode));
            CheckInteractionLayoutIndexed(scene.slider->GetValue() == initial, "canceled slider draft restores original value", static_cast<size_t>(mode));
            if (mode == 0)
                scene.slider->SetBounds(original);
            else if (mode == 1)
                scene.slider->SetVisible(true);
            else
                scene.slider->SetEnabled(true);
        }
    }

    // A hidden containing panel also cancels a child slider draft.
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "interaction-layout ancestor-cancel graphics pool");
        DxUi::EmbeddedHost view;
        Hr(view.Attach(graphics), "interaction-layout ancestor-cancel host attaches");
        auto root    = std::make_unique<DxUi::Panel>();
        auto* pane   = root->AddChild<DxUi::Panel>();
        auto* slider = pane->AddChild<DxUi::Slider>();
        slider->SetValue(35);
        slider->SetBounds(D2D1::RectF(24, 160, 440, 208));
        pane->SetBounds(D2D1::RectF(0, 0, 480, 240));
        view.Controls().SetRoot(std::move(root));
        Hr(view.Prepare(kWidth, kHeight), "interaction-layout ancestor-cancel initial paint");
        Check(view.DispatchPointer({DxUi::PointerAction::Down, 100, 184}), "interaction-layout ancestor slider draft starts");
        Check(view.DispatchPointer({DxUi::PointerAction::Move, 340, 184}), "interaction-layout ancestor slider draft changes");
        pane->SetVisible(false);
        Hr(view.PrepareInteraction(kWidth, kHeight), "interaction-layout hidden ancestor acknowledged");
        Check(view.Controls().GetCapturedControl() == nullptr, "interaction-layout hidden ancestor releases child capture");
        Check(slider->GetValue() == 35, "interaction-layout hidden ancestor restores child slider value");
    }

    // A splitter capture survives bounds changes to a sibling; the drag keeps using its captured control.
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "interaction-layout splitter graphics pool");
        DxUi::EmbeddedHost view;
        Hr(view.Attach(graphics), "interaction-layout splitter host attaches");
        auto root      = std::make_unique<DxUi::Panel>();
        auto* splitter = root->AddChild<DxUi::Splitter>();
        splitter->SetBounds(D2D1::RectF(0, 0, 480, 240));
        splitter->SetMinimumFirstPane(80);
        splitter->SetMinimumSecondPane(80);
        splitter->SetPosition(200);
        auto* sibling = root->AddChild<DxUi::Slider>();
        sibling->SetBounds(D2D1::RectF(24, 160, 440, 208));
        view.Controls().SetRoot(std::move(root));
        Hr(view.Prepare(kWidth, kHeight), "interaction-layout splitter initial paint");
        Check(view.DispatchPointer({DxUi::PointerAction::Down, 203, 120}), "interaction-layout splitter drag starts");
        Check(view.DispatchPointer({DxUi::PointerAction::Move, 260, 120}), "interaction-layout splitter drag previews");
        const float firstPosition = splitter->GetPosition();
        sibling->SetBounds(D2D1::RectF(24, 168, 440, 216));
        Hr(view.PrepareInteraction(kWidth, kHeight), "interaction-layout sibling bounds acknowledged during splitter drag");
        Check(view.Controls().GetCapturedControl() == splitter && splitter->IsDragging(), "interaction-layout sibling change preserves splitter capture");
        Check(view.DispatchPointer({DxUi::PointerAction::Move, 300, 120}), "interaction-layout splitter drag continues without paint");
        Check(splitter->GetPosition() > firstPosition, "interaction-layout continued splitter drag advances position");
    }

    // Resolve capture through the live tree before dereferencing: destroying the captured control is safe.
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "interaction-layout destroyed-control graphics pool");
        DxUi::EmbeddedHost view;
        Hr(view.Attach(graphics), "interaction-layout destroyed-control host attaches");
        auto root      = std::make_unique<DxUi::Panel>();
        auto* splitter = root->AddChild<DxUi::Splitter>();
        splitter->SetBounds(D2D1::RectF(0, 0, 480, 240));
        splitter->SetMinimumFirstPane(80);
        splitter->SetMinimumSecondPane(80);
        splitter->SetPosition(200);
        view.Controls().SetRoot(std::move(root));
        Hr(view.Prepare(kWidth, kHeight), "interaction-layout destroyed-control initial paint");
        Check(view.DispatchPointer({DxUi::PointerAction::Down, 203, 120}), "interaction-layout soon-destroyed control captures pointer");
        static_cast<void>(splitter);
        static_cast<DxUi::Panel*>(view.Controls().GetRoot())->ClearChildren();
        Hr(view.PrepareInteraction(kWidth, kHeight), "interaction-layout destroyed capture acknowledged");
        Check(view.Controls().GetCapturedControl() == nullptr, "interaction-layout destroyed captured control is released safely");
    }

    // A capture-loss callback can mutate the tree while acknowledgement is canceling a moved capture. Do not
    // acknowledge the revision that existed before that callback; return ERROR_RETRY and require a fresh arrange.
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "interaction-layout callback-mutation graphics pool");
        DxUi::EmbeddedHost view;
        Hr(view.Attach(graphics), "interaction-layout callback-mutation host attaches");
        auto root     = std::make_unique<DxUi::Panel>();
        auto* sibling = root->AddChild<DxUi::Label>(L"Sibling");
        sibling->SetBounds(D2D1::RectF(240, 120, 340, 180));
        auto* probe = root->AddChild<InteractionLayoutCaptureProbe>(sibling);
        probe->SetBounds(D2D1::RectF(0, 0, 100, 100));
        view.Controls().SetRoot(std::move(root));
        Hr(view.Prepare(kWidth, kHeight), "interaction-layout callback-mutation initial paint");
        Check(sibling->GetBounds().left == 240 && sibling->GetBounds().top == 120, "interaction-layout sibling begins at literal pre-callback bounds");
        Check(view.DispatchPointer({DxUi::PointerAction::Down, 30, 30}), "interaction-layout callback probe captures pointer");
        probe->SetBounds(D2D1::RectF(10, 10, 110, 110));
        Check(view.PrepareInteraction(kWidth, kHeight) == HRESULT_FROM_WIN32(ERROR_RETRY),
              "interaction-layout callback mutation rejects stale acknowledgement with ERROR_RETRY");
        Check(view.Controls().GetCapturedControl() == nullptr, "interaction-layout callback mutation leaves capture canceled");
        Check(sibling->GetBounds().left == 120 && sibling->GetBounds().top == 120, "interaction-layout capture-loss callback changed sibling geometry");
        Check(! view.DispatchPointer({DxUi::PointerAction::Down, 30, 30}), "interaction-layout callback mutation keeps input disabled");
        Hr(view.PrepareInteraction(kWidth, kHeight), "interaction-layout caller re-arranges callback-mutated geometry");
    }

    // Interaction preparation enables live hit testing but publishes neither text geometry nor new UIA bounds.
    {
        EmbeddedScene scene;
        Hr(scene.Initialize(gpu.device.get(), {}, true), "interaction-layout publication scene attaches");
        Hr(scene.view.Prepare(kWidth, 360), "interaction-layout publication initial paint");
        auto site = std::make_shared<InteractionLayoutAccessibilitySite>();
        const DxUi::EmbeddedAccessibilityPlacement placement{{0, 0, float(kWidth), 360}, true};
        Hr(scene.view.AttachAccessibility(site, 0x494e544552414354ull, placement), "interaction-layout accessibility snapshot attaches");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 280}), "interaction-layout text control receives focus");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 100, 280}), "interaction-layout text control releases pointer");
        Hr(scene.view.Prepare(kWidth, 360), "interaction-layout focused text geometry paint");
        Hr(scene.view.UpdateAccessibility(placement), "interaction-layout focused accessibility snapshot publishes");
        DxUi::EmbeddedTextInputSnapshot before{};
        Hr(scene.view.ReadTextInput(before), "interaction-layout reads last painted text geometry");
        Check(before.caretBoundsDip.has_value() && before.viewportBoundsDip.has_value(), "interaction-layout full paint publishes text geometry");
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
        Hr(scene.view.GetAccessibilityProvider(provider.put()), "interaction-layout obtains accessibility root");
        wil::com_ptr_nothrow<IRawElementProviderFragment> textProvider;
        Hr(provider->ElementProviderFromPoint(100, 280, textProvider.put()), "interaction-layout obtains held text provider");
        Check(textProvider != nullptr, "interaction-layout held text provider exists before bounds query");
        UiaRect oldBounds{};
        Hr(textProvider->get_BoundingRectangle(&oldBounds), "interaction-layout reads held text provider bounds");
        Check(std::abs(oldBounds.left - 24) < 0.01 && std::abs(oldBounds.top - 260) < 0.01 && std::abs(oldBounds.width - 416) < 0.01 &&
                  std::abs(oldBounds.height - 48) < 0.01,
              "interaction-layout held provider begins at literal painted text bounds");

        scene.text->SetBounds(D2D1::RectF(24, 310, 440, 358));
        Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 280}),
              "interaction-layout stale painted bounds reject input before acknowledgement");
        Hr(scene.view.PrepareInteraction(kWidth, 360), "interaction-layout text relocation acknowledged");
        DxUi::EmbeddedTextInputSnapshot during{};
        Hr(scene.view.ReadTextInput(during), "interaction-layout reads text state after interaction preparation");
        Check(! during.caretBoundsDip && ! during.viewportBoundsDip, "interaction-layout interaction prepare publishes no text geometry");
        D2D1_RECT_F range{};
        bool clipped = false;
        Check(scene.view.GetTextInputRangeBounds(during.revision, 0, 1, range, clipped) == S_FALSE, "interaction-layout text range geometry waits for paint");
        Check(scene.view.UpdateAccessibility(placement) == S_FALSE, "interaction-layout interaction prepare does not publish UIA geometry");
        UiaRect stillPublished{};
        Hr(textProvider->get_BoundingRectangle(&stillPublished), "interaction-layout rereads held provider after interaction preparation");
        Check(std::abs(stillPublished.left - 24) < 0.01 && std::abs(stillPublished.top - 260) < 0.01 && std::abs(stillPublished.width - 416) < 0.01 &&
                  std::abs(stillPublished.height - 48) < 0.01,
              "interaction-layout held UIA provider retains literal last-painted bounds");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 334}), "interaction-layout acknowledged live text bounds accept input");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Up, 100, 334}), "interaction-layout acknowledged live text bounds complete gesture");
    }

    // An extent or DPI mismatch disables input. Only a subsequent successful full preparation restores it.
    {
        EmbeddedScene scene;
        Hr(scene.Initialize(gpu.device.get()), "interaction-layout extent-mismatch scene attaches");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout extent-mismatch initial paint");
        Check(scene.view.PrepareInteraction(kWidth + 1, kHeight, 96) == S_FALSE, "interaction-layout wrong width is unavailable");
        Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 84}), "interaction-layout wrong width disables input");
        Check(scene.view.PrepareInteraction(kWidth, kHeight, 96) == S_FALSE, "interaction-layout correct old extent cannot restore input after width mismatch");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout full preparation restores input after width mismatch");
        Check(scene.view.PrepareInteraction(kWidth, kHeight, 120) == S_FALSE, "interaction-layout wrong DPI is unavailable");
        Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 84}), "interaction-layout wrong DPI disables input");
        Check(scene.view.PrepareInteraction(kWidth, kHeight, 96) == S_FALSE, "interaction-layout correct old DPI cannot restore input after DPI mismatch");
        Hr(scene.view.Prepare(kWidth, kHeight, 96), "interaction-layout full preparation restores input after DPI mismatch");
    }

    // A failed over-budget paint cannot be rehabilitated by acknowledging the last good dimensions.
    {
        EmbeddedScene scene;
        Hr(scene.Initialize(gpu.device.get()), "interaction-layout failed-paint scene attaches");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout failed-paint initial paint");
        Check(scene.view.Prepare(16384, 16384) == E_OUTOFMEMORY, "interaction-layout over-budget full preparation fails");
        Check(scene.view.PrepareInteraction(kWidth, kHeight) == S_FALSE, "interaction-layout failed paint cannot be acknowledged");
        Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 84}), "interaction-layout failed paint keeps input disabled");
    }

    // Hiding, zero extent and device replacement each require a new full paint before input returns.
    {
        EmbeddedScene scene;
        Hr(scene.Initialize(gpu.device.get()), "interaction-layout lifecycle scene attaches");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout lifecycle initial paint");
        scene.view.SetVisible(false);
        scene.view.SetVisible(true);
        Check(scene.view.PrepareInteraction(kWidth, kHeight) == S_FALSE, "interaction-layout hide and show require full preparation");
        Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 84}), "interaction-layout hide and show leave input disabled");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout paint after show");
        Check(scene.view.Prepare(0, 0) == S_FALSE, "interaction-layout zero extent suspends the view");
        Check(scene.view.PrepareInteraction(kWidth, kHeight) == S_FALSE, "interaction-layout zero extent cannot be acknowledged");
        Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 84}), "interaction-layout zero extent leaves input disabled");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout paint after zero extent");
        std::shared_ptr<DxUi::GraphicsDevice> replacement;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), replacement), "interaction-layout replacement graphics pool");
        Hr(scene.view.ReplaceDevice(replacement), "interaction-layout device replacement");
        Check(scene.view.PrepareInteraction(kWidth, kHeight) == S_FALSE, "interaction-layout replacement device requires full preparation");
        Check(! scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 84}), "interaction-layout replacement device leaves input disabled");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout paint after device replacement");
    }

    // A wrong-thread acknowledgement returns before touching owner-thread state.
    {
        EmbeddedScene scene;
        Hr(scene.Initialize(gpu.device.get()), "interaction-layout wrong-thread scene attaches");
        Hr(scene.view.Prepare(kWidth, kHeight), "interaction-layout wrong-thread initial paint");
        const auto before      = scene.view.GetStatistics();
        const bool dirtyBefore = scene.view.NeedsPreparation();
        HRESULT workerResult   = E_FAIL;
        std::thread worker([&] { workerResult = scene.view.PrepareInteraction(kWidth, kHeight); });
        worker.join();
        Check(workerResult == RPC_E_WRONG_THREAD, "interaction-layout wrong thread returns RPC_E_WRONG_THREAD");
        const auto after = scene.view.GetStatistics();
        Check(after.preparations == before.preparations && after.composites == before.composites && after.surfaceAllocations == before.surfaceAllocations &&
                  scene.view.NeedsPreparation() == dirtyBefore,
              "interaction-layout wrong-thread call leaves owner state unchanged");
        Check(scene.view.DispatchPointer({DxUi::PointerAction::Down, 100, 84}), "interaction-layout owner input remains enabled after wrong-thread call");
    }
}
